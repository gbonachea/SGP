#include "DockAnimation.h"
#include <QPauseAnimation>

DockAnimation::DockAnimation(QObject* parent)
    : QObject(parent)
{}

QSequentialAnimationGroup* DockAnimation::makeBounce(
    QObject* target, const QByteArray& property,
    qreal amplitude, int bounces)
{
    auto* group = new QSequentialAnimationGroup(target);

    for (int i = 0; i < bounces; ++i) {
        qreal amp = amplitude * (1.0 - (double)i / bounces);

        // Up
        auto* up = new QPropertyAnimation(target, property);
        up->setDuration(100);
        up->setStartValue(0.0);
        up->setEndValue(-amp);
        up->setEasingCurve(QEasingCurve::OutQuad);
        group->addAnimation(up);

        // Down
        auto* down = new QPropertyAnimation(target, property);
        down->setDuration(100);
        down->setStartValue(-amp);
        down->setEndValue(0.0);
        down->setEasingCurve(QEasingCurve::InQuad);
        group->addAnimation(down);

        // Brief pause between bounces
        if (i < bounces - 1) {
            group->addPause(30);
        }
    }

    return group;
}

QSequentialAnimationGroup* DockAnimation::makePop(
    QObject* target, const QByteArray& property, qreal currentScale)
{
    auto* group = new QSequentialAnimationGroup(target);

    // Compress
    auto* compress = new QPropertyAnimation(target, property);
    compress->setDuration(80);
    compress->setStartValue(currentScale);
    compress->setEndValue(0.88);
    compress->setEasingCurve(QEasingCurve::OutQuad);
    group->addAnimation(compress);

    // Pop out
    auto* expand = new QPropertyAnimation(target, property);
    expand->setDuration(180);
    expand->setStartValue(0.88);
    expand->setEndValue(1.18);
    expand->setEasingCurve(QEasingCurve::OutBack);
    group->addAnimation(expand);

    // Settle
    auto* settle = new QPropertyAnimation(target, property);
    settle->setDuration(120);
    settle->setStartValue(1.18);
    settle->setEndValue(1.0);
    settle->setEasingCurve(QEasingCurve::InOutQuad);
    group->addAnimation(settle);

    return group;
}

QPropertyAnimation* DockAnimation::makeSlide(
    QObject* target, const QByteArray& property,
    int from, int to, int duration)
{
    auto* anim = new QPropertyAnimation(target, property);
    anim->setDuration(duration);
    anim->setStartValue(from);
    anim->setEndValue(to);
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    return anim;
}

QPropertyAnimation* DockAnimation::makeFade(
    QObject* target, const QByteArray& property,
    qreal from, qreal to, int duration)
{
    auto* anim = new QPropertyAnimation(target, property);
    anim->setDuration(duration);
    anim->setStartValue(from);
    anim->setEndValue(to);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    return anim;
}
