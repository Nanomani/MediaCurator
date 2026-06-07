#include "engine/RemuxJob.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

#include <filesystem>

namespace Mc {

RemuxJob::RemuxJob(qint64 jobId, const QString& mkvmergePath,
				   const QStringList& args,
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
	// Extract output path (args[1] when args[0] == "-o") and input path (last arg)
	if (args.size() >= 2 && args.first() == "-o")
		m_outputPath = args.at(1);
	if (!args.isEmpty())
		m_inputPath = args.last();
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
				emit progressChanged(match.captured(1).toInt());
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

	qint64 savedBytes = 0;

	if (exitCode == 0 && !m_outputPath.isEmpty() && !m_inputPath.isEmpty()) {
		const qint64 outputSize = QFileInfo(m_outputPath).size();
		try {
			std::filesystem::rename(m_outputPath.toStdWString(),
			                        m_inputPath.toStdWString());
			savedBytes = qMax(0LL, m_originalSize - outputSize);
		} catch (const std::filesystem::filesystem_error& e) {
			m_log += QStringLiteral("\nRename failed: %1").arg(e.what());
			exitCode = -2;
		}

		if (exitCode == 0 && m_writeLog) {
			const QString logPath = m_inputPath + QStringLiteral(".mc-log");
			QFile logFile(logPath);
			if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
				QTextStream ts(&logFile);
				const QFileInfo fi(m_inputPath);
				ts << "MediaCurator Processing Report\n";
				ts << "==============================\n\n";
				ts << "File:    " << fi.fileName() << "\n";
				ts << "Path:    " << fi.absolutePath() << "\n";
				ts << "Date:    " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
				ts << "Before:  " << QString::number(m_originalSize) << " bytes ("
				   << QString::number(m_originalSize / 1048576.0, 'f', 1) << " MB)\n";
				ts << "After:   " << QString::number(outputSize) << " bytes ("
				   << QString::number(outputSize / 1048576.0, 'f', 1) << " MB)\n";
				if (savedBytes > 0)
					ts << "Saved:   " << QString::number(savedBytes) << " bytes ("
					   << QString::number(savedBytes / 1048576.0, 'f', 1) << " MB)\n";
				ts << "\n";
				if (!m_descriptionText.isEmpty()) {
					ts << "Removed Tracks\n";
					ts << "--------------\n";
					ts << m_descriptionText << "\n\n";
				}
				ts << "Command\n";
				ts << "-------\n";
				ts << m_mkvmergePath;
				for (const QString& arg : m_args)
					ts << " " << arg;
				ts << "\n";
				logFile.close();
			}
		}
	} else if (exitCode != 0 && !m_outputPath.isEmpty()) {
		QFile::remove(m_outputPath);
	}

	emit finished(exitCode, m_log, savedBytes);
}

} // namespace Mc
