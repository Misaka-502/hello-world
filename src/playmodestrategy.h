#ifndef PLAYMODE_STRATEGY_H
#define PLAYMODE_STRATEGY_H

#include <QString>

// Polymorphism: abstract strategy for selecting the next track index
// Three concrete implementations: ListLoop, SingleLoop, Random

class PlayModeStrategy
{
public:
    virtual ~PlayModeStrategy() = default;

    // Given current index and total track count, return the next index
    virtual int nextIndex(int current, int count) const = 0;

    // Display name for UI
    virtual QString name() const = 0;

    // Icon/emoji for UI
    virtual QString icon() const = 0;
};

// Cycle through list in order, wrap around at end
class ListLoopStrategy : public PlayModeStrategy
{
public:
    int nextIndex(int current, int count) const override;
    QString name() const override { return "List Loop"; }
    QString icon() const override { return QString::fromUtf8("\xF0\x9F\x94\x81"); } // 🔁
};

// Repeat the current track
class SingleLoopStrategy : public PlayModeStrategy
{
public:
    int nextIndex(int current, int count) const override;
    QString name() const override { return "Single Loop"; }
    QString icon() const override { return QString::fromUtf8("\xF0\x9F\x94\x82"); } // 🔂
};

// Pick a random track (different from current when possible)
class RandomStrategy : public PlayModeStrategy
{
public:
    int nextIndex(int current, int count) const override;
    QString name() const override { return "Shuffle"; }
    QString icon() const override { return QString::fromUtf8("\xF0\x9F\x94\x80"); } // 🔀
};

#endif // PLAYMODE_STRATEGY_H
