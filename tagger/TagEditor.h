#pragma once
#include <string>
#include <taglib/flacfile.h>
#include <taglib/tag.h>

class TagEditor {
public:
  explicit TagEditor(const std::string &filePath);
  ~TagEditor();

  bool load();
  bool save();

  std::string getTitle() const;
  std::string getArtist() const;
  std::string getAlbum() const;
  std::string getGenre() const;
  std::string getDate() const;

  int getTrackNumber() const;

  void setTitle(const std::string &title);
  void setArtist(const std::string &artist);
  void setAlbum(const std::string &album);
  void setGenre(const std::string &genre);
  void setTrackNumber(int track);
  void setDate(const std::string &date);

private:
  enum class FileType { Unknown, MP3, FLAC };

  struct Tags {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string date;
    int track = 0;
  };

  FileType detectFileType() const;

  std::string filename_;
  Tags tags_;
  bool modified_ = false;
  bool loaded_ = false;
};
