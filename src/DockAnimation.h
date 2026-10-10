#pragma once
#include <QObject>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>
#include <QEasingCurve>

/**
 * DockAnimation — factory for reusable dock animations.
 * All animations target a QObject property and are pre-configured
 * with macOS-like easing curves and durations.
 */
class DockAnimation : public QObject {
    Q_OBJECT
public:
    explicit DockAnimation(QObject* parent = nullptr);

    /**
     * Creates a bounce animation on `property` of `target`.
     * Mimics the macOS dock launch bounce: 4 bounces, decreasing amplitude.
     */
    static QSequentialAnimationGroup* makeBounce(QObject* target,
                                                  const QByteArray& property,
                                                  qreal amplitude = 14.0,
                                                  int bounces = 4);

    /**
     * Creates a smooth scale pop animation (0.9 → 1.15 → 1.0).
     * Used when launching or focusing an app.
     */
    static QSequentialAnimationGroup* makePop(QObject* target,
                                               const QByteArray& property,
                                               qreal currentScale = 1.0);

    /**
     * Creates a slide-in animation for auto-hide show/hide transitions.
     */
    static QPropertyAnimation* makeSlide(QObject* target,
                                          const QByteArray& property,
                                          int from, int to,
                                          int duration = 250);

    /**
     * Creates a fade animation (opacity).
     */
    static QPropertyAnimation* makeFade(QObject* target,
                                         const QByteArray& property,
                                         qreal from, qreal to,
                                         int duration = 200);
};
