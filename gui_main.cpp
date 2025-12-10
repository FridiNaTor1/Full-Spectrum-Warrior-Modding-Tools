#include "fsw_tool.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>

class MainWindow : public QWidget {
    Q_OBJECT

public:
    MainWindow() {
        setWindowTitle("FSW Modding Kit (C++ GUI)");
        auto *layout = new QVBoxLayout(this);

        auto *pathGroup = new QGroupBox("Game install directory", this);
        auto *pathLayout = new QHBoxLayout(pathGroup);
        installPathEdit = new QLineEdit(pathGroup);
        auto *browseBtn = new QPushButton("Browse…", pathGroup);
        pathLayout->addWidget(installPathEdit, 1);
        pathLayout->addWidget(browseBtn);
        layout->addWidget(pathGroup);

        connect(browseBtn, &QPushButton::clicked, this, &MainWindow::browseInstall);

        auto *buttonGroup = new QGroupBox("Patchers", this);
        auto *buttons = new QGridLayout(buttonGroup);

        addActionButton(buttons, 0, 0, "No downs limit",
                        [this] { runDllPatch(fsw::patch_no_downs_limit, "No downs limit applied."); });
        addActionButton(buttons, 0, 1, "GameSpy -> OpenSpy",
                        [this] { runDllPatch(fsw::patch_gamespy_to_openspy, "GameSpy patch done."); });
        addActionButton(buttons, 1, 0, "No mission failures",
                        [this] { runPakPatch(fsw::patch_no_mission_failures); });
        addActionButton(buttons, 1, 1, "Ammo 999",
                        [this] { runPakPatch(fsw::patch_ammo_999); });

        buttonGroup->setLayout(buttons);
        layout->addWidget(buttonGroup);

        auto *resolutionGroup = new QGroupBox("Write Resolution.cfg", this);
        auto *resLayout = new QHBoxLayout(resolutionGroup);
        resWidth = new QSpinBox(resolutionGroup);
        resHeight = new QSpinBox(resolutionGroup);
        resWidth->setRange(320, 10000);
        resHeight->setRange(240, 10000);
        resWidth->setValue(1920);
        resHeight->setValue(1080);
        auto *writeResBtn = new QPushButton("Write", resolutionGroup);
        resLayout->addWidget(new QLabel("Width:"));
        resLayout->addWidget(resWidth);
        resLayout->addWidget(new QLabel("Height:"));
        resLayout->addWidget(resHeight);
        resLayout->addStretch();
        resLayout->addWidget(writeResBtn);
        layout->addWidget(resolutionGroup);

        connect(writeResBtn, &QPushButton::clicked, this, &MainWindow::writeResolution);

        layout->addStretch();
    }

private:
    QLineEdit *installPathEdit;
    QSpinBox *resWidth;
    QSpinBox *resHeight;

    std::filesystem::path installDir() const {
        return std::filesystem::path(installPathEdit->text().toStdString());
    }

    void browseInstall() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select game install directory");
        if (!dir.isEmpty()) {
            installPathEdit->setText(dir);
        }
    }

    template <typename Fn>
    void addActionButton(QGridLayout *layout, int row, int col, const QString &label, Fn fn) {
        auto *btn = new QPushButton(label, this);
        layout->addWidget(btn, row, col);
        connect(btn, &QPushButton::clicked, this, fn);
    }

    template <typename Fn>
    void runDllPatch(Fn fn, const QString &successTitle) {
        std::filesystem::path dll = installDir() / "FSW.dll";
        if (installPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Missing install path", "Please select the game install directory.");
            return;
        }
        OperationResult res = fn(dll);
        showResult(res, successTitle);
    }

    template <typename Fn>
    void runPakPatch(Fn fn) {
        std::filesystem::path chapters = installDir() / "Chapters";
        if (installPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Missing install path", "Please select the game install directory.");
            return;
        }
        OperationResult res = fn(chapters);
        showResult(res, "PAK patch complete");
    }

    void showResult(const OperationResult &res, const QString &successTitle) {
        if (res.success) {
            QMessageBox::information(this, successTitle, QString::fromStdString(res.message));
        } else {
            QMessageBox::critical(this, "Error", QString::fromStdString(res.message));
        }
    }

    void writeResolution() {
        if (installPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Missing install path", "Please select the game install directory.");
            return;
        }
        OperationResult res = fsw::write_resolution(installDir(), resWidth->value(), resHeight->value());
        showResult(res, "Resolution written");
    }
};

#include "gui_main.moc"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}

