#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("CodeQuizzio");
    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("CodeQuizzio", "Main");

    // Dev aid: `cq_app --screenshot out.png` renders one frame and exits.
    const QStringList args = app.arguments();
    const int shot = static_cast<int>(args.indexOf("--screenshot"));
    if (shot >= 0 && shot + 1 < args.size()) {
        const QString path = args.at(shot + 1);
        QTimer::singleShot(700, &app, [&engine, path] {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            if (window) window->grabWindow().save(path);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
