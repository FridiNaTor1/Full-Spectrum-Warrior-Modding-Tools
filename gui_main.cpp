#include "fsw_tool.hpp"

#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QComboBox>
#include <QInputDialog>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <optional>
#include <algorithm>
#include <map>
#include <memory>
#include <set>

class ColorFieldWidget : public QWidget {
    Q_OBJECT
  public:
    explicit ColorFieldWidget(const QString &initial, QWidget *parent = nullptr) : QWidget(parent) {
        parsed_ = parseColor(initial);
        if (!parsed_) {
            parsed_ = {255, 255, 255, 255, true};
        }

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        edit_ = new QLineEdit(initial, this);
        pick_ = new QPushButton("Pick", this);
        slider_ = new QSlider(Qt::Horizontal, this);
        slider_->setRange(0, 255);
        example_ = new QLabel("EXAMPLE", this);
        example_->setFixedWidth(80);
        example_->setAlignment(Qt::AlignCenter);

        layout->addWidget(edit_, 2);
        layout->addWidget(pick_);
        layout->addWidget(slider_);
        layout->addWidget(example_);

        slider_->setEnabled(parsed_->hasAlpha);
        applyStateToWidgets();

        connect(edit_, &QLineEdit::textChanged, this, &ColorFieldWidget::onTextChanged);
        connect(pick_, &QPushButton::clicked, this, &ColorFieldWidget::onPick);
        connect(slider_, &QSlider::valueChanged, this, &ColorFieldWidget::onAlphaChanged);
    }

  signals:
    void valueChanged(const QString &text);

  private:
    struct ParsedColor {
        int r, g, b, a;
        bool hasAlpha;
    };

    std::optional<ParsedColor> parseColor(const QString &text) {
        QRegularExpression rgba("\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*");
        auto m = rgba.match(text);
        if (m.hasMatch()) {
            return ParsedColor{m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(),
                               m.captured(4).toInt(), true};
        }
        QRegularExpression rgb("\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*");
        m = rgb.match(text);
        if (!m.hasMatch()) {
            return std::nullopt;
        }
        return ParsedColor{m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(), 255,
                           false};
    }

    QString formatValue() const {
        if (!parsed_) {
            return edit_->text();
        }
        if (parsed_->hasAlpha) {
            return QString("%1, %2, %3, %4").arg(parsed_->r).arg(parsed_->g).arg(parsed_->b).arg(parsed_->a);
        }
        return QString("%1, %2, %3").arg(parsed_->r).arg(parsed_->g).arg(parsed_->b);
    }

    void applyStateToWidgets() {
        updating_ = true;
        if (parsed_) {
            edit_->setText(formatValue());
            if (parsed_->hasAlpha) {
                slider_->setValue(parsed_->a);
            }
            QColor c(parsed_->r, parsed_->g, parsed_->b, parsed_->a);
            QPalette pal = example_->palette();
            pal.setColor(QPalette::WindowText, c);
            example_->setPalette(pal);
        }
        updating_ = false;
        emit valueChanged(edit_->text());
    }

    void onTextChanged(const QString &text) {
        if (updating_) {
            return;
        }
        auto parsed = parseColor(text);
        if (parsed) {
            parsed_ = parsed;
            slider_->setEnabled(parsed_->hasAlpha);
            applyStateToWidgets();
        } else {
            emit valueChanged(text);
        }
    }

    void onPick() {
        QColor start(parsed_->r, parsed_->g, parsed_->b, parsed_->a);
        QColor chosen = QColorDialog::getColor(start, this, "Pick color");
        if (!chosen.isValid()) {
            return;
        }
        parsed_ = ParsedColor{chosen.red(), chosen.green(), chosen.blue(), parsed_->a, parsed_->hasAlpha};
        applyStateToWidgets();
    }

    void onAlphaChanged(int value) {
        if (updating_ || !parsed_ || !parsed_->hasAlpha) {
            return;
        }
        parsed_->a = value;
        applyStateToWidgets();
    }

    QLineEdit *edit_{};
    QPushButton *pick_{};
    QSlider *slider_{};
    QLabel *example_{};
    bool updating_{};
    std::optional<ParsedColor> parsed_;
};

class DescriptorEditorWidget : public QWidget {
    Q_OBJECT
  public:
    explicit DescriptorEditorWidget(QWidget *parent = nullptr) : QWidget(parent) {
        scroll_ = new QScrollArea(this);
        scroll_->setWidgetResizable(true);
        form_container_ = new QWidget(scroll_);
        form_layout_ = new QFormLayout(form_container_);
        form_layout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        scroll_->setWidget(form_container_);

        raw_edit_ = new QPlainTextEdit(this);
        raw_edit_->setReadOnly(true);
        raw_edit_->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto *layout = new QVBoxLayout(this);
        layout->addWidget(scroll_, 1);
        layout->addWidget(new QLabel("Raw descriptor body (read-only):"));
        layout->addWidget(raw_edit_, 1);
    }

    void setDescriptor(fsw::DescriptorRecord *rec, fsw::DescriptorModel *model) {
        rec_ = rec;
        model_ = model;
        clearForm();
        if (!rec_ || !model_) {
            raw_edit_->clear();
            return;
        }

        // priority ordering
        const std::vector<QString> priority = {"Name", "Script", "Unit", "Trigger", "Target", "WeaponType"};
        std::vector<QString> ordered_keys;
        for (const auto &p : priority) {
            if (rec_->key_values.count(p.toStdString())) {
                ordered_keys.push_back(p);
            }
        }
        for (const auto &kv : rec_->key_values) {
            QString k = QString::fromStdString(kv.first);
            if (std::find(ordered_keys.begin(), ordered_keys.end(), k) == ordered_keys.end()) {
                ordered_keys.push_back(k);
            }
        }

        for (const auto &k : ordered_keys) {
            QString key = k;
            auto vals = model_->options_for_key(key.toStdString());
            auto *combo = new QComboBox(this);
            combo->setEditable(true);
            for (const auto &v : vals) {
                combo->addItem(QString::fromStdString(v));
            }
            QString current;
            if (auto it = rec_->key_values.find(key.toStdString()); it != rec_->key_values.end()) {
                current = QString::fromStdString(it->second);
            }
            if (!current.isEmpty() && combo->findText(current) == -1) {
                combo->insertItem(0, current);
                combo->setCurrentIndex(0);
            } else {
                combo->setCurrentText(current);
            }
            connect(combo, &QComboBox::currentTextChanged, this,
                    [this, key](const QString &text) { onValueChanged(key, text); });
            form_layout_->addRow(key + ":", combo);
            combos_[key] = combo;
        }

        raw_edit_->setPlainText(QString::fromStdString(rec_->body));
    }

  private:
    void clearForm() {
        while (form_layout_->rowCount()) {
            form_layout_->removeRow(0);
        }
        combos_.clear();
    }

    void onValueChanged(const QString &key, const QString &text) {
        if (!rec_ || !model_) {
            return;
        }
        model_->replace_key(*rec_, key.toStdString(), text.toStdString());
        raw_edit_->setPlainText(QString::fromStdString(rec_->body));
    }

    QScrollArea *scroll_{};
    QWidget *form_container_{};
    QFormLayout *form_layout_{};
    QPlainTextEdit *raw_edit_{};
    std::map<QString, QComboBox *> combos_;
    fsw::DescriptorRecord *rec_{};
    fsw::DescriptorModel *model_{};
};

class DescriptorEditorWindow : public QMainWindow {
    Q_OBJECT
  public:
    DescriptorEditorWindow(const std::filesystem::path &pak, QWidget *parent = nullptr)
        : QMainWindow(parent) {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QString("Descriptor Editor - %1").arg(QString::fromStdString(pak.filename().string())));
        resize(1100, 700);
        try {
            model_ = std::make_unique<fsw::DescriptorModel>(pak);
        } catch (const std::exception &ex) {
            QMessageBox::critical(this, "Error", QString::fromStdString(ex.what()));
            close();
            return;
        }

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        list_ = new QTreeWidget(splitter);
        list_->setHeaderLabels({"Name", "Type", "#"});
        editor_ = new DescriptorEditorWidget(splitter);
        splitter->addWidget(list_);
        splitter->addWidget(editor_);
        splitter->setStretchFactor(1, 1);
        setCentralWidget(splitter);

        connect(list_, &QTreeWidget::itemSelectionChanged, this, &DescriptorEditorWindow::onSelectionChanged);

        auto *fileMenu = menuBar()->addMenu("&File");
        auto *saveAct = fileMenu->addAction("Save");
        connect(saveAct, &QAction::triggered, this, &DescriptorEditorWindow::save);

        populate();
    }

  private slots:
    void onSelectionChanged() {
        auto items = list_->selectedItems();
        if (items.isEmpty()) {
            editor_->setDescriptor(nullptr, nullptr);
            return;
        }
        auto *item = items.first();
        int idx = item->data(0, Qt::UserRole).toInt();
        editor_->setDescriptor(&model_->descriptors().at(static_cast<std::size_t>(idx)), model_.get());
    }

    void save() {
        try {
            model_->save();
            QMessageBox::information(this, "Saved", "PAK saved with descriptor updates.");
        } catch (const std::exception &ex) {
            QMessageBox::critical(this, "Error", QString::fromStdString(ex.what()));
        }
    }

  private:
    void populate() {
        list_->clear();
        for (const auto &rec : model_->descriptors()) {
            QString name = QString::fromStdString(rec.key_values.count("Name") ? rec.key_values.at("Name") : "");
            auto *item = new QTreeWidgetItem(list_, {name, QString::fromStdString(rec.desc_type),
                                                     QString::number(static_cast<int>(rec.index))});
            item->setData(0, Qt::UserRole, static_cast<int>(rec.index));
        }
        list_->resizeColumnToContents(0);
        list_->resizeColumnToContents(1);
        list_->resizeColumnToContents(2);
    }

    std::unique_ptr<fsw::DescriptorModel> model_;
    QTreeWidget *list_{};
    DescriptorEditorWidget *editor_{};
};

class RulesEditorWidget : public QWidget {
    Q_OBJECT
  public:
    explicit RulesEditorWidget(QWidget *parent = nullptr) : QWidget(parent) {
        scroll_ = new QScrollArea(this);
        scroll_->setWidgetResizable(true);
        form_container_ = new QWidget(scroll_);
        form_layout_ = new QFormLayout(form_container_);
        form_layout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        scroll_->setWidget(form_container_);

        auto *layout = new QVBoxLayout(this);
        layout->addWidget(scroll_, 1);
    }

    void setSection(fsw::RuleSection *section, fsw::RulesModel *model) {
        section_ = section;
        model_ = model;
        clearForm();
        if (!section_ || !model_) {
            return;
        }

        for (const auto &key : section_->fields_order) {
            auto &field = section_->fields.at(key);
            QString key_q = QString::fromStdString(key);
            bool is_color = key_q.toLower().endsWith("color") || key_q.toLower().endsWith("col");
            if (is_color) {
                auto *widget = new ColorFieldWidget(QString::fromStdString(field.value), this);
                connect(widget, &ColorFieldWidget::valueChanged, this, [this, &field](const QString &text) {
                    model_->update_field(field, text.toStdString());
                });
                form_layout_->addRow(key_q + ":", widget);
            } else {
                auto *edit = new QLineEdit(QString::fromStdString(field.value), this);
                connect(edit, &QLineEdit::textChanged, this, [this, &field](const QString &text) {
                    model_->update_field(field, text.toStdString());
                });
                form_layout_->addRow(key_q + ":", edit);
            }
        }
    }

  private:
    void clearForm() {
        while (form_layout_->rowCount()) {
            form_layout_->removeRow(0);
        }
    }

    QScrollArea *scroll_{};
    QWidget *form_container_{};
    QFormLayout *form_layout_{};
    fsw::RuleSection *section_{};
    fsw::RulesModel *model_{};
};

class RulesEditorWindow : public QMainWindow {
    Q_OBJECT
  public:
    RulesEditorWindow(const std::filesystem::path &pak, QWidget *parent = nullptr) : QMainWindow(parent) {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QString("Game Rules Editor - %1").arg(QString::fromStdString(pak.filename().string())));
        resize(900, 700);
        try {
            model_ = std::make_unique<fsw::RulesModel>(pak);
        } catch (const std::exception &ex) {
            QMessageBox::critical(this, "Error", QString::fromStdString(ex.what()));
            close();
            return;
        }

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        list_ = new QListWidget(splitter);
        editor_ = new RulesEditorWidget(splitter);
        splitter->addWidget(list_);
        splitter->addWidget(editor_);
        splitter->setStretchFactor(1, 1);
        setCentralWidget(splitter);

        for (const auto &sec : model_->sections()) {
            list_->addItem(QString::fromStdString(sec.name));
        }
        connect(list_, &QListWidget::currentRowChanged, this, &RulesEditorWindow::onSectionChanged);

        auto *toolbar = addToolBar("Rules");
        auto *saveAct = toolbar->addAction("Save");
        connect(saveAct, &QAction::triggered, this, &RulesEditorWindow::save);

        if (list_->count() > 0) {
            list_->setCurrentRow(0);
        }
    }

  private slots:
    void onSectionChanged(int row) {
        if (row < 0 || row >= list_->count()) {
            editor_->setSection(nullptr, nullptr);
            return;
        }
        auto &sec = model_->sections().at(static_cast<std::size_t>(row));
        editor_->setSection(const_cast<fsw::RuleSection *>(&sec), model_.get());
    }

    void save() {
        try {
            auto new_data = model_->build_new_data();
            if (new_data.size() != model_->raw_data().size()) {
                QMessageBox::critical(this, "Size mismatch", "Rebuilt rules data changed PAK size.");
                return;
            }
            model_->save();
            QMessageBox::information(this, "Saved", "Rules saved into PAK.");
        } catch (const std::exception &ex) {
            QMessageBox::critical(this, "Error", QString::fromStdString(ex.what()));
        }
    }

  private:
    std::unique_ptr<fsw::RulesModel> model_;
    QListWidget *list_{};
    RulesEditorWidget *editor_{};
};

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
        addActionButton(buttons, 1, 1, "999 grenades, M203, and smoke grenades",
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

        auto *editGroup = new QGroupBox("Editors", this);
        auto *editLayout = new QHBoxLayout(editGroup);
        auto *descBtn = new QPushButton("Edit descriptors", editGroup);
        auto *rulesBtn = new QPushButton("Edit game rules", editGroup);
        editLayout->addWidget(descBtn);
        editLayout->addWidget(rulesBtn);
        layout->addWidget(editGroup);

        connect(descBtn, &QPushButton::clicked, this, &MainWindow::openDescriptorEditor);
        connect(rulesBtn, &QPushButton::clicked, this, &MainWindow::openRulesEditor);

        layout->addStretch();
    }

  private:
    QLineEdit *installPathEdit{};
    QSpinBox *resWidth{};
    QSpinBox *resHeight{};

    std::filesystem::path installDir() const { return std::filesystem::path(installPathEdit->text().toStdString()); }

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
        if (installPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Missing install path", "Please select the game install directory.");
            return;
        }
        std::filesystem::path dll = installDir() / "FSW.dll";
        OperationResult res = fn(dll);
        showResult(res, successTitle);
    }

    template <typename Fn>
    void runPakPatch(Fn fn) {
        if (installPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Missing install path", "Please select the game install directory.");
            return;
        }
        std::filesystem::path chapters = installDir() / "Chapters";
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

    std::vector<std::filesystem::path> listPaks(const std::filesystem::path &install) {
        auto chapters = install / "Chapters";
        auto paks = fsw::iter_pak_files(chapters);
        if (paks.empty()) {
            QMessageBox::critical(this, "No PAKs found", QString::fromStdString(chapters.string()));
        }
        return paks;
    }

    void openDescriptorEditor() {
        if (installPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Missing install path", "Please select the game install directory.");
            return;
        }
        auto paks = listPaks(installDir());
        if (paks.empty()) {
            return;
        }
        QStringList items;
        for (const auto &p : paks) {
            items << QString::fromStdString(p.filename().string());
        }
        bool ok = false;
        QString choice = QInputDialog::getItem(this, "Select PAK", "PAK:", items, 0, false, &ok);
        if (!ok) {
            return;
        }
        auto idx = items.indexOf(choice);
        auto *win = new DescriptorEditorWindow(paks[static_cast<std::size_t>(idx)], this);
        win->show();
    }

    void openRulesEditor() {
        if (installPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Missing install path", "Please select the game install directory.");
            return;
        }
        auto paks = listPaks(installDir());
        if (paks.empty()) {
            return;
        }
        QStringList items;
        for (const auto &p : paks) {
            items << QString::fromStdString(p.filename().string());
        }
        bool ok = false;
        QString choice = QInputDialog::getItem(this, "Select PAK", "PAK:", items, 0, false, &ok);
        if (!ok) {
            return;
        }
        auto idx = items.indexOf(choice);
        auto *win = new RulesEditorWindow(paks[static_cast<std::size_t>(idx)], this);
        win->show();
    }
};

#include "gui_main.moc"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
