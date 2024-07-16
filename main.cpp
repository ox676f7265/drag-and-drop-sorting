#include <QApplication>
#include <QWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QFileInfo>
#include <QPushButton>
#include <QDir>
#include <QMessageBox>

#include <map>
#include <vector>
#include <set>
#include <string>
#include <algorithm>

class DragDropWidget : public QWidget {
    Q_OBJECT

public:
    DragDropWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setAcceptDrops(true);
        QVBoxLayout *layout = new QVBoxLayout(this);

        infoLabel = new QLabel("Перетащите файлы сюда", this);
        treeWidget = new QTreeWidget(this);
        treeWidget->setHeaderLabels({"Группа", "Файлы"});
        resetButton = new QPushButton("Сбросить", this);
        createDirsButton = new QPushButton("Создать директории по группам", this);

        layout->addWidget(infoLabel);
        layout->addWidget(treeWidget);
        layout->addWidget(resetButton);
        layout->addWidget(createDirsButton);

        connect(resetButton, &QPushButton::clicked, this, &DragDropWidget::resetGroups);
        connect(createDirsButton, &QPushButton::clicked, this, &DragDropWidget::createDirectoriesForGroups);
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override {
        QList<QUrl> urls = event->mimeData()->urls(); // получает список url из события перетаскивания
        if (urls.isEmpty()) {
            return;
        }

        // группировка файлов по имени исключая постфикс
        std::map<std::string, std::set<std::string>> groups;
        std::set<std::string> ungroupedFiles;

        for (const QUrl &url : urls) {
            QString filePath = url.toLocalFile();
            QString fileName = QFileInfo(filePath).fileName();
            std::string fileNameStr = fileName.toStdString();

            size_t pos = fileNameStr.find_last_of("-");
            std::string baseName;
            if (pos != std::string::npos) {
                baseName = fileNameStr.substr(0, pos);
            } else {
                baseName = fileNameStr.substr(0, fileNameStr.find_last_of('.'));
            }

            for (char &c : baseName) {
                c = std::tolower(static_cast<unsigned char>(c));
            }
            groups[baseName].insert(fileNameStr);
        }

        // обновление дерева
        for (const auto &group : groups) {
            std::string groupName = group.first;
            const std::set<std::string>& files = group.second;

            bool hasBaseFile = false;
            for (const std::string &file : files) {
                if (file == groupName + ".jpg" || file == groupName + ".jpeg" || file == groupName + ".png") {
                    hasBaseFile = true;
                    break;
                }
            }

            // если нет базового файла - несгруппировано
            if (!hasBaseFile) {
                ungroupedFiles.insert(files.begin(), files.end());
                continue;
            }

            // если один файл или нет разделителя - несгруппировано
            if (files.size() == 1 || groupName == *files.begin()) {
                ungroupedFiles.insert(*files.begin());
                continue;
            }

            QTreeWidgetItem *groupItem = findGroupItem(QString::fromStdString(groupName));
            if (!groupItem) { // если groupItem == nullptr , создаётся элемент группы
                groupItem = new QTreeWidgetItem(treeWidget);
                groupItem->setText(0, QString::fromStdString(groupName));
                groupItem->setText(1, QString::number(files.size()) + " файла(-ов)");
            }

            for (const std::string &file : files) {
                if (!isFileAlreadyAdded(groupItem, QString::fromStdString(file))) {
                    QTreeWidgetItem *fileItem = new QTreeWidgetItem(groupItem);
                    fileItem->setText(0, "");
                    fileItem->setText(1, QString::fromStdString(file));
                }
            }

            // обновление количества файлов в группе
            groupItem->setText(1, QString::number(groupItem->childCount()) + " файла(-ов)");
        }

        // группа несгруппировано
        if (!ungroupedFiles.empty()) {
            QTreeWidgetItem *ungroupedItem = findGroupItem("несгруппировано");
            if (!ungroupedItem) {
                ungroupedItem = new QTreeWidgetItem(treeWidget);
                ungroupedItem->setText(0, "несгруппировано");
                ungroupedItem->setText(1, QString::number(ungroupedFiles.size()) + " файла(-ов)");
            }

            for (const std::string &file : ungroupedFiles) {
                if (!isFileAlreadyAdded(ungroupedItem, QString::fromStdString(file))) {
                    QTreeWidgetItem *fileItem = new QTreeWidgetItem(ungroupedItem);
                    fileItem->setText(0, "");
                    fileItem->setText(1, QString::fromStdString(file));
                }
            }

            ungroupedItem->setText(1, QString::number(ungroupedItem->childCount()) + " файла(-ов)");
        }
    }

private:
    QLabel *infoLabel;
    QTreeWidget *treeWidget;
    QPushButton *resetButton;
    QPushButton *createDirsButton;

    QTreeWidgetItem* findGroupItem(const QString& groupName) {
        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = treeWidget->topLevelItem(i);
            if (item->text(0) == groupName) {
                return item;
            }
        }
        return nullptr;
    }

    bool isFileAlreadyAdded(QTreeWidgetItem* groupItem, const QString& fileName) {
        for (int i = 0; i < groupItem->childCount(); ++i) {
            QTreeWidgetItem *item = groupItem->child(i);
            if (item->text(1) == fileName) {
                return true;
            }
        }
        return false;
    }

    void resetGroups() {
        treeWidget->clear();
    }

    void createDirectoriesForGroups() {
        QDir currentDir = QDir::current();
        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem *groupItem = treeWidget->topLevelItem(i);
            QString groupName = groupItem->text(0);
            QString groupDirPath = currentDir.absoluteFilePath(groupName);
            QDir groupDir(groupDirPath);
            if (!groupDir.exists()) {
                if (!currentDir.mkdir(groupName)) {
                    QMessageBox::warning(this, "Ошибка", "Не удалось создать директорию для группы: " + groupName);
                    continue;
                }
            }
            for (int j = 0; j < groupItem->childCount(); ++j) {
                QTreeWidgetItem *fileItem = groupItem->child(j);
                QString fileName = fileItem->text(1);
                QString sourceFilePath = currentDir.absoluteFilePath(fileName);
                QString destFilePath = groupDir.absoluteFilePath(fileName);
                if (!QFile::copy(sourceFilePath, destFilePath)) {
                    QMessageBox::warning(this, "Ошибка", "Не удалось скопировать файл: " + fileName + " в " + groupDirPath);
                }
            }
        }
        QMessageBox::information(this, "Успех", "Директории для групп успешно созданы.");
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    DragDropWidget window;
    window.setWindowTitle("Drag-and-Drop группировка файлов");
    window.resize(400, 400);
    window.show();
    return app.exec();
}

#include "main.moc"
