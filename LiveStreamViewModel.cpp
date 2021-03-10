#include "pch.h"
#include "LiveStreamViewModel.h"

#include "LiveStreamSourceModel.h"
#include "LiveStreamViewLayoutModel.h"
#include "AudioOutput.h"

LiveStreamViewModel::LiveStreamViewModel(QObject *parent)
    :QAbstractListModel(parent)
{
    audio_out_ = new AudioOutput;
    audio_out_->moveToThread(&audio_thread_);
    connect(&audio_thread_, &QThread::finished, audio_out_, &QObject::deleteLater);
    audio_thread_.start();

    LoadFromFile();
    if (view_info_.empty())
        view_info_.push_back(LiveStreamViewInfo(0, 0, 1, 1, -1, nullptr, audio_out_));
}

LiveStreamViewModel::~LiveStreamViewModel()
{
    audio_thread_.exit();
    audio_thread_.wait();

    SaveToFile();
}

int LiveStreamViewModel::rowCount(const QModelIndex &) const
{
    return view_info_.size();
}

QVariant LiveStreamViewModel::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
    case Qt::DisplayRole:
        if (index.column() == 0 && index.row() >= 0 && index.row() < (int)view_info_.size())
            return QVariant::fromValue(view_info_[index.row()]);
        break;
    default:
        break;
    }

    return QVariant();
}

void LiveStreamViewModel::setSourceModel(LiveStreamSourceModel *new_source_model)
{
    if (source_model_ != new_source_model)
    {
        if (source_model_)
        {
            disconnect(source_model_, &LiveStreamSourceModel::deleteSource, this, &LiveStreamViewModel::OnDeleteSource);
        }
        source_model_ = new_source_model;
        if (source_model_)
        {
            connect(source_model_, &LiveStreamSourceModel::deleteSource, this, &LiveStreamViewModel::OnDeleteSource); //Should be direct
        }
        emit sourceModelChanged();
    }
}

void LiveStreamViewModel::resetLayout(LiveStreamViewLayoutModel *layout_model)
{
    if (!layout_model)
        return;
    std::vector<LiveStreamViewInfo> new_view_info;
    for (const auto &item : layout_model->LayoutItems())
        new_view_info.push_back(LiveStreamViewInfo(item.row(), item.column(), item.rowSpan(), item.columnSpan(), -1, nullptr, audio_out_));

    if (Q_LIKELY(source_model_))
        source_model_->DeactivateAllSource();

    setRows(layout_model->rows());
    setColumns(layout_model->columns());
    beginResetModel();
    view_info_ = std::move(new_view_info);
    endResetModel();
}

void LiveStreamViewModel::setSource(int index_row, int source_id)
{
    if (index_row >= 0 && index_row < (int)view_info_.size() && Q_LIKELY(source_model_))
    {
        LiveStreamViewInfo &view_info = view_info_[index_row];

        LiveStreamSource *source = source_model_->ActivateAndGetSource(source_id);
        if (source)
        {
            if (view_info.sourceId() != -1)
                source_model_->DeactivateSingleSource(view_info.sourceId());
            view_info.setSource(source_id, source);
            emit dataChanged(index(index_row), index(index_row));
        }
    }
}

void LiveStreamViewModel::swapSource(int index_1, int index_2)
{
    if (index_1 >= 0 && index_1 < (int)view_info_.size() && index_2 >= 0 && index_2 < (int)view_info_.size())
    {
        LiveStreamViewInfo &view_info_1 = view_info_[index_1];
        LiveStreamViewInfo &view_info_2 = view_info_[index_2];

        int source_id_1 = view_info_1.sourceId();
        LiveStreamSource *source_1 = view_info_1.source();
        int source_id_2 = view_info_2.sourceId();
        LiveStreamSource *source_2 = view_info_2.source();

        view_info_1.setSource(source_id_2, source_2);
        emit dataChanged(index(index_1), index(index_1));
        view_info_2.setSource(source_id_1, source_1);
        emit dataChanged(index(index_2), index(index_2));
    }
}

void LiveStreamViewModel::OnDeleteSource(int source_id)
{
    for (int i = 0; i < (int)view_info_.size(); ++i)
    {
        auto &view_info = view_info_[i];
        if (view_info.sourceId() == source_id)
        {
            view_info.setSource(-1, nullptr);
            emit dataChanged(index(i), index(i)); //Should be direct
        }
    }
}

void LiveStreamViewModel::LoadFromFile()
{
    QFile file("saved_view_layout.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    if (file.bytesAvailable() >= 0x1000)
        return;
    QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject())
        return;
    QJsonObject json_object = json.object();

    int rows = json_object.value("rows").toDouble(-1);
    int columns = json_object.value("columns").toDouble(-1);
    if (rows < 0 || columns < 0)
        return;
    rows_ = rows;
    columns_ = columns;
    QJsonArray json_array = json_object.value("layout").toArray();

    for (const auto &item : json_array)
    {
        if (!item.isObject())
            continue;
        QJsonObject item_object = item.toObject();

        int row = item_object.value("row").toDouble(-1);
        int column = item_object.value("column").toDouble(-1);
        int rowSpan = item_object.value("rowSpan").toDouble(-1);
        int columnSpan = item_object.value("columnSpan").toDouble(-1);
        if (row < 0 || column < 0 || rowSpan <= 0 || columnSpan <= 0)
            continue;
        view_info_.push_back(LiveStreamViewInfo(row, column, rowSpan, columnSpan, -1, nullptr, audio_out_));
    }
}

void LiveStreamViewModel::SaveToFile()
{
    QJsonArray json_array;
    for (const auto &p : view_info_)
    {
        QJsonObject item;
        item["row"] = p.row();
        item["column"] = p.column();
        item["rowSpan"] = p.rowSpan();
        item["columnSpan"] = p.columnSpan();
        json_array.push_back(item);
    }

    QJsonObject json_object;
    json_object["rows"] = rows_;
    json_object["columns"] = columns_;
    json_object["layout"] = std::move(json_array);

    QFile file("saved_view_layout.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    file.write(QJsonDocument(json_object).toJson(QJsonDocument::Compact));
    file.close();
}
