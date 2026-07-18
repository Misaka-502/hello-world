#include "playmodestrategy.h"
#include <QRandomGenerator>

int ListLoopStrategy::nextIndex(int current, int count) const
{
    if (count <= 1)
        return 0;
    return (current + 1) % count;
}

int SingleLoopStrategy::nextIndex(int current, int /*count*/) const
{
    return current;
}

int RandomStrategy::nextIndex(int current, int count) const
{
    if (count <= 1)
        return 0;
    if (count == 2)
        return (current + 1) % 2;

    int next = current;
    while (next == current) {
        next = QRandomGenerator::global()->bounded(count);
    }
    return next;
}
