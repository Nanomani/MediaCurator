#pragma once

#include "core/DatabaseManager.h"
#include <QAbstractTableModel>
#include <QList>

namespace Mc {

/**
 * McTrackTableModel — flat table model that joins files + streams.
 * Each row represents one stream/track, with the parent file's info in the first columns.
 */
class McTrackTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        Col_Filename = 0,
        Col_Container,
        Col_TrackType,
        Col_Language,
        Col_Codec,
        Col_Channels,
        Col_Resolution,
        Col_Bitrate,
        Col_Title,
        Col_Duration,
        Col_FileSize,
        Col_HDR,
        Col_Default,
        Col_Forced,
        ColumnCount
    };

    explicit McTrackTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void reload();

private:
    struct Row {
        FileRecord   file;
        StreamRecord stream;
    };

    QList<Row> m_rows;
};

} // namespace Mc
