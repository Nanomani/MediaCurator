#pragma once
#include <QByteArray>
#include <QDialog>
#include <QHash>
#include <QNetworkAccessManager>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QNetworkReply;

namespace Mc {

class ImdbSearchDialog : public QDialog {
	Q_OBJECT
public:
	ImdbSearchDialog(const QString& videoPath,
	                  const QString& suggestedTitle,
	                  const QString& existingImdbId,
	                  const QString& tmdbApiKey,
	                  QWidget* parent = nullptr);
	~ImdbSearchDialog() override;

	QString    selectedImdbId()     const;
	QString    selectedTitle()      const;
	int        selectedYear()       const;
	QString    selectedPosterPath() const;
	QByteArray selectedImageData()  const;


private slots:
	void onSearch();
	void onResultSelectionChanged();

protected:
	void keyPressEvent(QKeyEvent* event) override;
	bool eventFilter(QObject* obj, QEvent* ev) override;

private:
	void setStatusText(const QString& text, bool isError = false);

	QString m_videoPath;
	QString m_tmdbApiKey;

	QLineEdit*   m_searchEdit   = nullptr;
	QPushButton* m_btnSearch    = nullptr;
	QLabel*      m_statusLabel  = nullptr;
	QListWidget* m_resultsList  = nullptr;
	QLineEdit*   m_imdbIdEdit   = nullptr;
	QPushButton* m_btnSave      = nullptr;

	QNetworkAccessManager*      m_nam = nullptr;
	QNetworkReply*              m_searchReply  = nullptr;
	QNetworkReply*              m_extIdsReply  = nullptr;
	QHash<int, QNetworkReply*>  m_thumbReplyByRow;   // row → in-flight thumbnail reply
	QHash<int, QNetworkReply*>  m_prefetchByRow;     // row → in-flight external_ids prefetch
	QHash<int, QString>         m_imdbIdByRow;       // row → cached IMDb ID

	QString                m_selectedTitle;
	QString                m_selectedPosterPath;
	QHash<int, QByteArray> m_thumbDataByRow;   // row → raw image bytes
	int     m_selectedYear     = 0;
	bool    m_acceptAfterFetch = false;
};

} // namespace Mc
