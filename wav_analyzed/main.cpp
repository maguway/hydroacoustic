#include "wav_loader.h"
#include "map.h"

#include <windows.h>
#include <commdlg.h>
#include <events.h>
#include <detail.h>

#include <fstream>
#include <string>

#pragma comment(lib, "comdlg32.lib")

namespace
{
    std::string selectWavFile()
    {
        char filename[MAX_PATH] = {0};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "WAV files (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = "Выберите WAV-файл";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) return std::string(filename);
        return "";
    }

    std::string baseName(const std::string& path)
    {
        auto pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }

    std::string dirName(const std::string& path)
    {
        auto pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? "" : path.substr(0, pos);
    }

    std::string stemName(const std::string& path)
    {
        std::string n = baseName(path);
        auto pos = n.find_last_of('.');
        return (pos == std::string::npos) ? n : n.substr(0, pos);
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    const std::string path = selectWavFile();
    if (path.empty()) return 0;

    sd::WavData wav;
    try
    {
        wav = sd::loadWav(path);
    }
    catch (const std::exception& e)
    {
        std::string msg = "Ошибка загрузки:\n";
        msg += e.what();
        MessageBoxA(nullptr, msg.c_str(), "sound_digit", MB_OK | MB_ICONERROR);
        return 2;
    }

    const std::string outPath = dirName(path) + "\\" + stemName(path) + ".hear.txt";

    std::ofstream f(outPath);
    if (!f)
    {
        MessageBoxA(nullptr, "Не удалось записать отчёт.", "sound_digit",
                    MB_OK | MB_ICONERROR);
        return 3;
    }

    sd::writeMap(f, wav);
    f.close();

    // Второй файл — события.
    const std::string evPath = dirName(path) + "\\" + stemName(path) + ".events.txt";
    std::ofstream fe(evPath);
    if (fe)
    {
        sd::writeEvents(fe, wav);
        fe.close();
    }

    // Третий файл — детали.
    const std::string dtPath = dirName(path) + "\\" + stemName(path) + ".detail.txt";
    std::ofstream fd(dtPath);
    if (fd)
    {
        sd::writeDetails(fd, wav);
        fd.close();
    }

    ShellExecuteA(nullptr, "open", "notepad.exe",
                  ("\"" + outPath + "\"").c_str(),
                  nullptr, SW_SHOWNORMAL);

    ShellExecuteA(nullptr, "open", "notepad.exe",
              ("\"" + evPath + "\"").c_str(),
              nullptr, SW_SHOWNORMAL);

    ShellExecuteA(nullptr, "open", "notepad.exe",
              ("\"" + dtPath + "\"").c_str(),
              nullptr, SW_SHOWNORMAL);

    f.close();
    return 0;
}
