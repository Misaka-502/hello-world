#ifndef TRACK_H
#define TRACK_H

#include <QString>
#include <QUrl>

Class Track
{
public:
    Track() = default;
    explicit Track(const QString& filePath);
    QString filePath() const { return m_filePath; }
    QString title() const { return m_title.isEmpty() ? m_fileName : m_title; }
    QString fileName() const { return m_fileName; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    int duration() const { return m_duration; }
    QUrl url() const { return QUrl::fromLocalFile(m_filePath); }
    QString displayText() const;

    bool isValid() const;

    
    void setDuration(int ms);
    void setTitle(const QString& title);
    void setArtist(const QString& artist);
    void setAlbum(const QString& album);

    
    bool operator==(const Track& other) const;
private:
    QString m_filePath;      
    QString m_fileName;      
    QString m_title;         
    QString m_artist;        
    QString m_album;         
    int m_duration = 0;      
};
#endif