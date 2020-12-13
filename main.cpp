#include <QApplication>
#include "videoreader.h"
#include "videowriter.h"
#include "QDebug"
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    VideoReader lReader("/home/oscar/Desktop/Proje/record.mp4");
    VideoWriter lWriter("/home/oscar/Desktop/old-guard.mp4");
    lReader.init();
    lWriter.init(lReader.widht(), lReader.height(), lReader.fps(), 12000);
    while (lReader.readNext()) {
        lWriter.addFrame(lReader.currentData());
        qDebug() << "continue";
    }
    lWriter.finish();
    qDebug() << "finished";

    return app.exec();
}
