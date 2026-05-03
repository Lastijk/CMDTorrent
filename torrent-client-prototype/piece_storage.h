#pragma once

#include "torrent_file.h"
#include "piece.h"
#include <queue>
#include <string>
#include <unordered_set>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <vector>

/*
 * Хранилище информации о частях скачиваемого файла.
 * В этом классе отслеживается информация о том, какие части файла осталось скачать
 */
class PieceStorage {
public:
    PieceStorage(const TorrentFile& tf, const std::filesystem::path& outputDirectory, size_t piecesToDownload);

    /*
     * Отдает указатель на следующую часть файла, которую надо скачать
     */
    PiecePtr GetNextPieceToDownload();

    /*
     * Эта функция вызывается из PeerConnect, когда скачивание одной части файла завершено.
     * В рамках данного задания требуется очистить очередь частей для скачивания как только хотя бы одна часть будет успешно скачана.
     */
    void PieceProcessed(const PiecePtr& piece);

    /*
     * Остались ли нескачанные части файла?
     */
    bool QueueIsEmpty() const;

    /*
     * Сколько частей файла было сохранено на диск
     */
    size_t PiecesSavedToDiscCount() const;

    /*
     * Сколько частей файла всего
     */
    size_t TotalPiecesCount() const;

    /*
     * Закрыть поток вывода в файл
     */
    void CloseOutputFile();

    /*
     * Отдает список номеров частей файла, которые были сохранены на диск
     */
    const std::vector<size_t>& GetPiecesSavedToDiscIndices() const;

    /*
     * Сколько частей файла в данный момент скачивается
     */
    size_t PiecesInProgressCount() const;

private:
    std::queue<PiecePtr> remainPieces_;
    std::vector<PiecePtr> allPieces_;
    mutable std::mutex mutex_;
    std::fstream outputFile_;
    std::filesystem::path outputFilePath_;
    size_t pieceLength_ = 0;
    size_t totalPiecesCount_ = 0;
    size_t piecesToDownload_ = 0;
    bool queueClosed_ = false;
    std::vector<size_t> piecesSavedToDiscIndices_;
    std::unordered_set<size_t> piecesInProgress_;
    std::unordered_set<size_t> piecesSavedToDisc_;

    /*
     * Сохраняет данную скачанную часть файла на диск.
     * Сохранение всех частей происходит в один выходной файл. Позиция записываемых данных зависит от индекса части
     * и размера частей. Данные, содержащиеся в части файла, должны быть записаны сразу в правильную позицию.
     */
    void SavePieceToDisk(const PiecePtr& piece);

    void RefillQueue();
};
