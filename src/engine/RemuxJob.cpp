#include "engine/RemuxJob.h"
#include "core/ExternalTools.h"

#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QThreadPool>

#include <filesystem>

#ifdef Q_OS_WIN
#include <windows.h>
static FILETIME toFileTime(const QDateTime& dt)
{
	const qint64 ns100 = (dt.toMSecsSinceEpoch() + Q_INT64_C(11644473600000)) * 10000;
	FILETIME ft;
	ft.dwLowDateTime  = static_cast<DWORD>(ns100 & 0xFFFFFFFF);
	ft.dwHighDateTime = static_cast<DWORD>((ns100 >> 32) & 0xFFFFFFFF);
	return ft;
}
// Restore both the creation and last-modified timestamps from the original file
// on the output file so the processed file doesn't bubble up as "newest" in
// media libraries that sort by Date Modified.
static void preserveTimestamps(const QString& target, const QDateTime& origCreated, const QDateTime& origModified)
{
	HANDLE h = CreateFileW(reinterpret_cast<const wchar_t*>(target.utf16()),
	                       FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING,
	                       FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return;
	FILETIME ftCreated  = origCreated.isValid()  ? toFileTime(origCreated)  : FILETIME{};
	FILETIME ftModified = origModified.isValid() ? toFileTime(origModified) : FILETIME{};
	SetFileTime(h,
	            origCreated.isValid()  ? &ftCreated  : nullptr,
	            nullptr,
	            origModified.isValid() ? &ftModified : nullptr);
	CloseHandle(h);
}
#endif

namespace Mc {

RemuxJob::RemuxJob(qint64 jobId, const QString& mkvmergePath,
				   const QStringList& args,
				   const QString& sourceFilePath,
				   const QString& descriptionText,
				   bool writeLog,
				   QObject* parent)
	: QObject(parent)
	, m_jobId(jobId)
	, m_mkvmergePath(mkvmergePath)
	, m_args(args)
	, m_descriptionText(descriptionText)
	, m_writeLog(writeLog)
{
	// Extract temp output path (args[1] when args[0] == "-o")
	if (args.size() >= 2 && args.first() == QLatin1String("-o"))
		m_outputPath = args.at(1);

	// Strip .tmp suffix to get the final destination path
	m_finalOutputPath = m_outputPath.endsWith(QLatin1String(".tmp"))
		? m_outputPath.left(m_outputPath.length() - 4)
		: m_outputPath;

	// Real input filesystem path — ISO files have a "bluray://" or "dvd://" prefix
	// that mkvmerge needs but the filesystem does not understand.
	// sourceFilePath is captured before any sidecar args are appended to the command,
	// so it always refers to the primary MKV/ISO source rather than a sidecar file.
	if (sourceFilePath.startsWith(QLatin1String("bluray://")))
		m_inputPath = sourceFilePath.mid(9);
	else if (sourceFilePath.startsWith(QLatin1String("dvd://")))
		m_inputPath = sourceFilePath.mid(6);
	else
		m_inputPath = sourceFilePath;
}

RemuxJob::~RemuxJob()
{
	if (m_process && m_process->state() != QProcess::NotRunning) {
		m_process->kill();
		m_process->waitForFinished(3000);
	}
}

void RemuxJob::run()
{
	m_originalSize = QFileInfo(m_inputPath).size();

	m_process = new QProcess(this);
	m_process->setProcessChannelMode(QProcess::MergedChannels);
	ExternalTools::applyBackgroundPriority(m_process);

	connect(m_process, &QProcess::readyRead,
	        this, &RemuxJob::onReadyRead);
	connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
	        this, [this](int code, QProcess::ExitStatus) { onProcessFinished(code); });

	m_process->start(m_mkvmergePath, m_args);
}

void RemuxJob::cancel()
{
	if (m_process && m_process->state() != QProcess::NotRunning)
		m_process->kill();
}

void RemuxJob::onReadyRead()
{
	static const QRegularExpression progressRe(R"(Progress:\s*(\d+)%)");

	m_readBuf += m_process->readAll();

	// mkvmerge uses \r (not \n) to overwrite the progress line in a terminal.
	// QProcess::canReadLine() only fires on \n, so split on both delimiters.
	int pos = 0;
	for (int i = 0; i < m_readBuf.size(); ++i) {
		const char c = m_readBuf.at(i);
		if (c == '\r' || c == '\n') {
			const QString line = QString::fromUtf8(m_readBuf.mid(pos, i - pos)).trimmed();
			pos = i + 1;
			if (line.isEmpty()) continue;
			m_log += line + '\n';
			const auto match = progressRe.match(line);
			if (match.hasMatch())
				emit progressChanged(match.captured(1).toInt(), QFileInfo(m_outputPath).size());
		}
	}
	m_readBuf = m_readBuf.mid(pos);
}

void RemuxJob::onProcessFinished(int exitCode)
{
	// Drain any bytes that didn't end with \r/\n
	m_readBuf += m_process->readAll();
	if (!m_readBuf.isEmpty()) {
		m_log += QString::fromUtf8(m_readBuf).trimmed() + '\n';
		m_readBuf.clear();
	}

	// Detect the dangerous warning: a requested track ID was not found in the file.
	// This can mean the track layout has shifted since the job was created, which
	// risks removing the wrong tracks. We keep the .tmp and let the user review.
	static const QRegularExpression mismatchRe(
		R"(track with the ID \d+ was requested but not found)",
		QRegularExpression::CaseInsensitiveOption);
	m_hasTrackMismatch = mismatchRe.match(m_log).hasMatch();

	// mkvmerge exit codes: 0 = success, 1 = warnings (output still valid), 2 = error
	if ((exitCode == 0 || exitCode == 1) && !m_outputPath.isEmpty() && !m_inputPath.isEmpty()) {
		if (m_hasTrackMismatch) {
			// Leave .tmp on disk — JobQueue will probe it and ask the user to verify
			// before committing or discarding.
			emit finished(exitCode, m_log, 0);
			return;
		}

		// Renaming/deleting the file and writing the log are pure filesystem work —
		// can be slow on network shares — so run them off the UI thread and marshal
		// the result back via a queued invocation, keeping `finished` emitted from
		// this object's own (UI) thread same as a synchronous call site would expect.
		const QString     outputPath      = m_outputPath;
		const QString     finalOutputPath = m_finalOutputPath;
		const QString     inputPath       = m_inputPath;
		const qint64      originalSize    = m_originalSize;
		const QString     log             = m_log;
		const bool        writeLog        = m_writeLog;
		const QString     mkvmergePath    = m_mkvmergePath;
		const QStringList args            = m_args;
		const QString     descriptionText = m_descriptionText;

		QThreadPool::globalInstance()->start(
		    [this, outputPath, finalOutputPath, inputPath, originalSize, log,
		     writeLog, mkvmergePath, args, descriptionText, exitCode]() mutable {
			QString finishLog  = log;
			qint64  savedBytes = 0;

			const qint64 outputSize = QFileInfo(outputPath).size();

			// Capture the original file's timestamps before we rename/delete it
			const QFileInfo origFi(inputPath);
			const QDateTime origCreated  = origFi.birthTime();
			const QDateTime origModified = origFi.lastModified();

			const bool isInPlace = (finalOutputPath == inputPath);
			try {
				// Rename temp file to final destination (same as input for MKV in-place)
				std::filesystem::rename(outputPath.toStdWString(),
				                        finalOutputPath.toStdWString());
				// Restore the original file's timestamps on the new output
#ifdef Q_OS_WIN
				preserveTimestamps(finalOutputPath, origCreated, origModified);
#endif
				if (!isInPlace) {
					// Non-MKV conversion (mp4/avi/iso → mkv): delete the original
					QFile::remove(inputPath);
				}
				savedBytes = qMax(0LL, originalSize - outputSize);
			} catch (const std::filesystem::filesystem_error& e) {
				// On SMB/NFS shares the rename can succeed on disk but still throw
				// (the server commits the operation then returns an ambiguous status).
				// Verify the actual filesystem state before treating it as a failure.
				const bool destExists = QFileInfo::exists(finalOutputPath);
				const bool srcGone    = !QFileInfo::exists(outputPath);
				if (destExists && srcGone) {
					// Rename succeeded despite the error — treat as success.
					savedBytes = qMax(0LL, originalSize - outputSize);
					finishLog += QStringLiteral("\nNote: rename reported a network error but the file is in the correct state (%1)").arg(e.what());
				} else {
					finishLog += QStringLiteral("\nRename failed: %1").arg(e.what());
					exitCode = -2;
				}
			}

			if ((exitCode == 0 || exitCode == 1) && writeLog) {
				const QString logPath = finalOutputPath + QStringLiteral(".mc-log");
				QFile logFile(logPath);
				if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
					QTextStream ts(&logFile);
					const QFileInfo fi(finalOutputPath);
					ts << "MediaCurator Processing Report\n";
					ts << "==============================\n\n";
					ts << "File:    " << fi.fileName() << "\n";
					ts << "Path:    " << fi.absolutePath() << "\n";
					ts << "Date:    " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
					ts << "Before:  " << QString::number(originalSize) << " bytes ("
					   << QString::number(originalSize / 1048576.0, 'f', 1) << " MB)\n";
					ts << "After:   " << QString::number(outputSize) << " bytes ("
					   << QString::number(outputSize / 1048576.0, 'f', 1) << " MB)\n";
					if (savedBytes > 0)
						ts << "Reclaimed: " << QString::number(savedBytes) << " bytes ("
						   << QString::number(savedBytes / 1048576.0, 'f', 1) << " MB)\n";
					ts << "\n";
					if (!descriptionText.isEmpty()) {
						ts << "Removed Tracks\n";
						ts << "--------------\n";
						ts << descriptionText << "\n\n";
					}
					ts << "Command\n";
					ts << "-------\n";
					ts << mkvmergePath;
					for (const QString& arg : args)
						ts << " " << arg;
					ts << "\n";
					logFile.close();
				}
			}

			QMetaObject::invokeMethod(this, [this, exitCode, finishLog, savedBytes] {
				emit finished(exitCode, finishLog, savedBytes);
			}, Qt::QueuedConnection);
		});
		return;
	}

	if (exitCode != 0 && !m_outputPath.isEmpty()) {
		const QString outputPath = m_outputPath;
		const QString log        = m_log;
		QThreadPool::globalInstance()->start([this, outputPath, log, exitCode]() {
			QFile::remove(outputPath);
			QMetaObject::invokeMethod(this, [this, exitCode, log] {
				emit finished(exitCode, log, 0);
			}, Qt::QueuedConnection);
		});
		return;
	}

	emit finished(exitCode, m_log, 0);
}

} // namespace Mc
