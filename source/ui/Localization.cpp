#include "ui/Localization.hpp"
#include <unordered_map>
namespace br {
std::string tr(const std::string& key, Language language) {
    static const std::unordered_map<std::string, std::string> ru = {
        {"Library", "Библиотека"},
        {"Recent", "Недавние"},
        {"Favorites", "Избранное"},
        {"Settings", "Настройки"},
        {"Continue Reading", "Продолжить чтение"},
        {"Search", "Поиск"},
        {"Open", "Открыть"},
        {"Back", "Назад"},
        {"Close Book", "Закрыть книгу"},
        {"Contents", "Оглавление"},
        {"Theme", "Тема"},
        {"Font size", "Размер шрифта"},
        {"Reading mode", "Режим чтения"},
        {"Orientation", "Ориентация"},
        {"Horizontal", "Горизонтальная"},
        {"Vertical", "Вертикальная"},
        {"Light", "Светлая"},
        {"Dark", "Тёмная"},
        {"Sepia", "Сепия"},
        {"Page", "Страница"},
        {"Chapter", "Глава"},
        {"Progress", "Прогресс"},
        {"Language", "Язык"},
        {"BOOK READER", "ЧИТАЛКА"},
        {"Your library. Wherever you are.", "Ваша библиотека всегда с вами."},
        {"Open a book from Library first", "Сначала откройте книгу в библиотеке"},
        {"Exit", "Выход"},
        {"RECENT BOOKS", "НЕДАВНИЕ КНИГИ"},
        {"Home", "Главная"},
        {"Title", "Название"},
        {"Author", "Автор"},
        {"Date", "Дата"},
        {"Format", "Формат"},
        {"Last opened", "Открытие"},
        {"Sort", "Сортировка"},
        {"Refresh", "Обновить"},
        {"Unknown author", "Автор неизвестен"},
        {"Unstar", "Убрать"},
        {"Star", "В избранное"},
        {"Scanning microSD...", "Сканирование microSD…"},
        {"No books", "Книг нет"},
        {"Previous", "Назад"},
        {"Next", "Далее"},
        {"books", "книг"},
        {"BOOK OPTIONS", "ДЕЙСТВИЯ С КНИГОЙ"},
        {"Toggle favorite", "В избранное / убрать"},
        {"SEARCH TITLE", "ПОИСК ПО НАЗВАНИЮ"},
        {"Space", "Пробел"},
        {"Backspace", "Удалить"},
        {"Clear", "Очистить"},
        {"Done", "Готово"},
        {"Line spacing", "Интервал"},
        {"Margins", "Поля"},
        {"Brightness", "Яркость"},
        {"Scroll", "Прокрутка"},
        {"Pages", "Страницы"},
        {"Font", "Шрифт"},
        {"Auto", "Авто"},
        {"System", "Системный"},
        {"Serif", "С засечками"},
        {"Sans", "Без засечек"},
        {"Mono", "Моноширинный"},
        {"Alignment", "Выравнивание"},
        {"Left", "Слева"},
        {"Center", "По центру"},
        {"Right", "Справа"},
        {"PDF layout is fixed.", "Вёрстка PDF фиксирована."},
        {"Brightness dims this app only.", "Яркость меняется только в читалке."},
        {"Book", "Книга"},
        {"Zoom -", "Масштаб −"},
        {"Zoom +", "Масштаб +"},
        {"Font -", "Шрифт −"},
        {"Font +", "Шрифт +"},
        {"Favorite", "В избранное"},
        {"Resume reading", "Продолжить чтение"},
        {"X: menu · B: back · L/R: chapter", "X: меню · B: назад · L/R: глава"},
        {"ZL/ZR: 10 pages · +: settings", "ZL/ZR: 10 страниц · +: настройки"},
        {"Pinch / double tap: zoom · Drag: pan", "Щипок / двойное касание: масштаб"},
        {"This book has no table of contents.", "В этой книге нет оглавления."},
        {"Loading...", "Загрузка…"},
        {"OK", "ОК"},
        {"Cannot save progress: check microSD space",
         "Не удалось сохранить: проверьте место на microSD"},
        {"Not enough graphics memory", "Недостаточно графической памяти"},
        {"Cannot open book", "Не удалось открыть книгу"},
        {"Not enough memory", "Недостаточно памяти"},
        {"Unsupported format", "Формат не поддерживается"},
        {"Invalid EPUB file", "Повреждённый EPUB"},
        {"Invalid FB2 file", "Повреждённый FB2"},
        {"Rendering failed", "Ошибка отображения страницы"},
        {"Book size limit", "Книга превышает допустимый размер"},
        {"Password-protected books are not supported", "Книги с паролем не поддерживаются"},
        {"Cannot write cache", "Не удалось записать кэш на microSD"},
        {"Cancelled", "Отменено"},
        {"No book is open", "Книга не открыта"},
        {"Cannot create BookReader folders", "Не удалось создать папки BookReader"},
        {"No Cyrillic font. Add a TTF to fonts/",
         "Нет кириллического шрифта. Добавьте TTF в fonts/"}};
    if (language == Language::Russian) {
        auto i = ru.find(key);
        if (i != ru.end())
            return i->second;
    }
    return key;
}
std::string localizedError(const std::string& message, Language language) {
    if (language == Language::English)
        return message;
    auto exact = tr(message, language);
    if (exact != message)
        return exact;
    if (message.find("memory") != std::string::npos || message.find("alloc") != std::string::npos)
        return tr("Not enough memory", language);
    if (message.find("limit") != std::string::npos || message.find("exceeds") != std::string::npos)
        return tr("Book size limit", language);
    if (message.find("render") != std::string::npos ||
        message.find("Rendering") != std::string::npos)
        return tr("Rendering failed", language);
    if (message.find("EPUB") != std::string::npos)
        return tr("Invalid EPUB file", language);
    if (message.find("cache") != std::string::npos)
        return tr("Cannot write cache", language);
    if (message.find("FB2") != std::string::npos)
        return tr("Invalid FB2 file", language);
    if (message.find("font") != std::string::npos)
        return tr("No Cyrillic font. Add a TTF to fonts/", language);
    return tr("Cannot open book", language);
}
} // namespace br
