#pragma once

#include <string>
#include <memory>

class TagEditor {
public:
    TagEditor(const std::string& filename);
    ~TagEditor();

    bool load();
    bool save();

    std::string getTitle() const;
    std::string getArtist() const;
    std::string getAlbum() const;
    std::string getDate() const;
    std::string getGenre() const;
    int getTrackNumber() const;

    void setTitle(const std::string& title);
    void setArtist(const std::string& artist);
    void setAlbum(const std::string& album);
    void setDate(const std::string& date);
    void setGenre(const std::string& genre);
    void setTrackNumber(int track);

private:
    std::string filename_;
    struct Tags {
        std::string title;
        std::string artist;
        std::string album;
        std::string date;
        std::string genre;
        int track = 0;
    } tags_;
    bool modified_;
    bool loaded_;

    enum class FileType { MP3, FLAC, Unknown };
    FileType detectFileType() const;
};
