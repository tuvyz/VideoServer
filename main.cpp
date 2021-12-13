#include <QCoreApplication>
#include "opencv2/opencv.hpp"
#include <QDebug>
#include <thread>
#include <vector>
#include <QDateTime>
#include <QDateTime>
#include <QFile>


static std::mutex Locker;
#define LOCK Locker.lock();
#define UNLOCK Locker.unlock();

#define DELETE(x) {delete x; x = nullptr;}








#include <chrono>
class MyTime {
private:
    std::chrono::time_point<std::chrono::steady_clock> begin = std::chrono::steady_clock::now();
public:
    MyTime () {
        
    }
    MyTime (uint8_t measurementNumber1or2, bool output2Console = 1) {
        operator ()(measurementNumber1or2, output2Console);
    }
    double operator ()(uint8_t measurementNumber1or2, bool output2Console = 1) {
        switch (measurementNumber1or2) {
        case 1: {
            begin = std::chrono::steady_clock::now(); // Первый замер времени
            return 1;
        }
        case 2: {
            auto end = std::chrono::steady_clock::now(); // Второй замер времени
            auto elapsed_mc = std::chrono::duration_cast
                    <std::chrono::microseconds>(end - begin); // Разница в микросекундах;
            double delay = elapsed_mc.count();
            
            if (output2Console == 1) { // Вывод в консоль по-умолчанию
                if (delay >= 1000000)
                    qDebug() << delay / 1000000 << "секунд";
                if (delay >= 1000 && delay < 1000000)
                    qDebug() << delay / 1000 << "миллисекунд";
                if (delay < 1000)
                    qDebug() << delay << "микросекунд";
            }
            return delay;
        }
        default:
            return 0;
        }
    }
    static void sleep(int delayInMicroseconds, bool processEvents = 0)
    {
        if (delayInMicroseconds <= 0)
            return;
        MyTime time(1);
        while (time(2, false) < delayInMicroseconds) {
            if (processEvents == 1)
                qApp->processEvents();
        }
    }
    static void sleepWhile(const std::function<bool()> &predicate,  bool processEvents = 0) {
        while (predicate() == true) {
            if (processEvents == 1)
                qApp->processEvents();
        }
    }
    
    static std::string sec2hmsStr(int second) {
        float hour = float(second) / 3600;
        float min = 60 * (hour - floor(hour));
        float sec = 60 * (min - floor(min));
        int hourInt = floor(hour), Min_int = floor(min), Sec_int = floor(sec);
        std::string hourStr = std::to_string(hourInt);
        if (hourInt < 10)
            hourStr = "0" + hourStr;
        std::string Min_str = std::to_string(Min_int);
        if (Min_int < 10)
            Min_str = "0" + Min_str;
        std::string Sec_str = std::to_string(Sec_int);
        if (Sec_int < 10)
            Sec_str = "0" + Sec_str;
        
        return (hourStr + ":" + Min_str + ":" + Sec_str);
    }
};






















// Видеосервер принимает видео с камер и сохраняет его по соответствующим 
// файлам, каждый длительностью по durationOneSegment
class VideoServer
{
    
public:
    

    // Инициализация
    VideoServer(std::string videoStorageDirectory, int durationOneSegment = 10) {
        this->videoStorageDirectory = videoStorageDirectory;
        this->durationOneSegment = durationOneSegment;
    }
    
    
    
    
    // Представление камеры
    struct Camera {
        std::string name;
        std::string address;
        uint numberRecordedKadr = 0;
        std::atomic<bool> isRun = 0;
        std::atomic<bool> stopSignal = 0;
        uint fps;
    };
    
    
    
    
    // Получить список с информацией о подключённых камерах
    const std::vector<Camera*> getCameraList() {
        return cameraList;
    }
    
    
    
    
    // Подключение камеры
    void connectionCamera(std::string address, const std::string cameraName = "Camera", uint fps = 25) {
        Camera *camera = new Camera;
        cv::VideoCapture videoCapture;
        if (videoCapture.open(address)) {
            videoCapture.release();
            // Добавление индекса к имени, если такой уже существует
            std::string newCameraName = cameraName;
            for (int i = 1;; i++) {
                auto findResult = find_if(begin(cameraList), end(cameraList), [newCameraName](const Camera *camera) {
                    return camera->name == newCameraName;
                });
                if (findResult != cameraList.end())
                    newCameraName = cameraName + '(' + std::to_string(i) + ')';
                else
                    break;
            }
            camera->name = newCameraName;
            camera->address = address;
            camera->fps = fps;
            camListMut.lock();
            cameraList.push_back(camera);
            camListMut.unlock();
            qDebug() << "Connected" << QString::fromStdString(camera->name);
        } else
            qDebug() << "Failed to connect" << QString::fromStdString(camera->name);
    }
    
    
    
    
    // Запуск видеосервера
    void run() {
        static std::mutex mtx;
        std::unique_lock<std::mutex> unMtx(mtx);
        
        for (auto *camera : cameraList)
            run(camera);
    }
    void run(Camera *camera) {
        static std::mutex mtx;
        std::unique_lock<std::mutex> unMtx(mtx);

        if (camera->isRun == 0) {
            camera->isRun = 1;
            camera->stopSignal = 0;
        } else
            return;
        
        std::thread th([camera, this](){

            cv::VideoCapture videoCapture;
            cv::VideoWriter videoWriter;
            std::string path;
            
            // Открытие камеры
            if (!videoCapture.open(camera->address)) {
                camera->isRun = 0;
                qDebug() << "Device " + QString::fromStdString(camera->name) + " cannot be connected";
                return;
            }
            
            
            // Иницилизация нового хранилища адресов обрезков
            videosListMtx.lock();
            std::shared_ptr<ChoppedVideo> videos(new ChoppedVideo);
            videos->duration = durationOneSegment;
            videos->cameraName = camera->name;
            videos->cameraAddress = camera->address;
            videos->recordStartTime = QDateTime::currentDateTime();
            videoList.push_back(videos);
            videosListMtx.unlock();
            
            
            
            while (camera->stopSignal == 0) {                    
                // Захват кадра
                cv::Mat frame;
                videoCapture >> frame;
                if (frame.cols == 0) {
                    camera->isRun = 0;
                    qDebug() << "Lost connection to " + QString::fromStdString(camera->name);
                    return;
                }
                
                // Заканчиваем писать в файл и начинаем в другой
                if (camera->numberRecordedKadr > durationOneSegment * camera->fps) {
                    videoWriter.release();
                    camera->numberRecordedKadr = 0;
                    videos->addresses.push_back(path);
                }
                
                // Открытие нового видеофайла
                if (camera->numberRecordedKadr == 0) {
                    
                    // Поиск свободного имени для файла
                    static std::mutex fileMtx;
                    fileMtx.lock();
                    path = videoStorageDirectory + camera->name;
                    int i = 0;
                    while (QFile::exists(QString::fromStdString(path + ".mp4")))
                        path = videoStorageDirectory + camera->name + '#' + std::to_string(++i);
                    path += ".mp4";
                    fileMtx.unlock();
                    
                    videoWriter.open(path, cv::VideoWriter::fourcc('m','p','4','v'), camera->fps,
                                     cv::Size(frame.cols, frame.rows));
                }
                
                // Запись в файл
                videoWriter.write(frame);
                camera->numberRecordedKadr++;
            }
            camera->isRun = 0;
            videos->durationLast = (.0f + camera->numberRecordedKadr) / camera->fps;
            videos->addresses.push_back(path);
        });
        th.detach();
    }
    
    
    
    
    // Остановка работы видеосервера
    void stop() {
        static std::mutex mtx;
        std::unique_lock<std::mutex> unMtx(mtx);
        
        for (auto *camera : cameraList)
            stop(camera);
    }
    void stop(Camera *camera) {
        static std::mutex mtx;
        std::unique_lock<std::mutex> unMtx(mtx);
        
        camera->stopSignal = 1;
    }
    
    
    
    
    // Структура с адресами обрезков одного видео
    struct ChoppedVideo {
        std::vector<std::string> addresses; // Список адресов
        QDateTime recordStartTime; // Время начала записи
        int duration; // Сколько длится одно видео секундах
        int durationLast; // Сколько длится последнее видео
        std::string cameraName;
        std::string cameraAddress;
    };
    
    
    
    
    // Сборка видео из обрезков
    void buildChoppedVideo(ChoppedVideo choppedVideo, std::string savePath) {
        cv::VideoCapture videoCapture;
        cv::VideoWriter videoWriter;
        for (auto address : choppedVideo.addresses) {
            if (!videoCapture.open(address))
                break;
            while (videoCapture.get(cv::CAP_PROP_POS_FRAMES) != videoCapture.get(cv::CAP_PROP_FRAME_COUNT)) {
                cv::Mat image;
                videoCapture >> image;
                if (!videoWriter.isOpened()) {
                    uint fps = videoCapture.get(cv::CAP_PROP_FPS);
                    videoWriter.open(savePath, cv::VideoWriter::fourcc('m','p','4','v'), fps,
                                     cv::Size(image.cols, image.rows));
                }
                videoWriter.write(image);
            }
        }
    }
    
    
    
    
    // Получить список всех видосов
    const std::vector<ChoppedVideo> getVideoList() {
        std::unique_lock<std::mutex> uni(videosListMtx);
        std::vector<ChoppedVideo> constAllSavedVideos;
        for (auto video : videoList)
            constAllSavedVideos.push_back(*video);
        return constAllSavedVideos;
    }   
    
    
    
private:
    
    std::vector<Camera*> cameraList; // Список подключённых камер
    std::mutex camListMut;
    
    std::string videoStorageDirectory; // Директория сохранения
    uint durationOneSegment; // Продолжительность одного фрагмента в секундах

    std::vector<std::shared_ptr<ChoppedVideo>> videoList; // Список всех видео с их адресами обрезков
    std::mutex videosListMtx;

};


















int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    
    
    int durationOneSegment = 10; // Длина отрезка видео в секундах
    VideoServer videoServer("D:/VIDEO_TEST/", durationOneSegment);
    videoServer.connectionCamera("rtsp://admin:@192.168.1.12:554/mode=real&idc=1&ids=1", "Первая", 25);
    videoServer.connectionCamera("rtsp://admin:@192.168.1.11:554/mode=real&idc=1&ids=1", "Вторая", 25);
    videoServer.connectionCamera("rtsp://admin:@192.168.1.10:554/mode=real&idc=1&ids=1", "Третья", 25);
 //   videoServer.connectionCamera("D:/SNOW.mp4", "Virtual");
  //  videoServer.connectionCamera("D:/Chn.mp4", "Virtual");
 //   videoServer.connectionCamera("D:/NewKADR.mp4", "Virtual");
    videoServer.run();
    
    
    
    //std::this_thread::sleep_for(std::chrono::seconds(16));
    //videoServer.stop();
    //videoServer.buildChoppedVideo(videoServer.getVideoList().front(), "D:/VIDEO_TEST/TEST.mp4"); //      !!! Нет проверки на существование файла
    
    
    return a.exec();
}





































