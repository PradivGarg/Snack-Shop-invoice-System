#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set date edit to show calendar and today's date
    ui->dateEdit->setCalendarPopup(true);
    ui->dateEdit->setDate(QDate::currentDate());
    ui->dateEdit->setDisplayFormat("yyyy-MM-dd");

    // Setup items table
    ui->ItemsTable->setColumnCount(3);
    QStringList headers = {"Item Name", "Quantity", "Price"};
    ui->ItemsTable->setHorizontalHeaderLabels(headers);
    ui->ItemsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->ItemsTable->verticalHeader()->setVisible(false);
    ui->ItemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->ItemsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);

    // Initialize database connection
    if (!initDatabase()) {
        QMessageBox::critical(this, "Database Error", "Failed to connect or initialize the database. Application will exit.");
        qApp->quit();
    }


}

MainWindow::~MainWindow()
{
    if (db.isOpen())
        db.close();

    delete ui;
}

bool MainWindow::initDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("snackshop.db"); // SQLite database file

    if (!db.open()) {
        qDebug() << "Database open error:" << db.lastError().text();
        return false;
    }

    QSqlQuery query;

    // Create invoices table if not exists
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS invoices ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "customer_name TEXT NOT NULL, "
            "date TEXT NOT NULL"
            ");"
            )) {
        qDebug() << "Failed to create invoices table:" << query.lastError().text();
        return false;
    }

    // Create invoice_items table if not exists
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS invoice_items ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "invoice_id INTEGER NOT NULL, "
            "item_name TEXT NOT NULL, "
            "quantity INTEGER NOT NULL, "
            "price REAL NOT NULL, "
            "FOREIGN KEY (invoice_id) REFERENCES invoices(id) ON DELETE CASCADE"
            ");"
            )) {
        qDebug() << "Failed to create invoice_items table:" << query.lastError().text();
        return false;
    }

    return true;
}

void MainWindow::on_addItemButton_clicked()
{
    int newRow = ui->ItemsTable->rowCount();
    ui->ItemsTable->insertRow(newRow);

    ui->ItemsTable->setItem(newRow, 0, new QTableWidgetItem(""));
    ui->ItemsTable->setItem(newRow, 1, new QTableWidgetItem("1"));
    ui->ItemsTable->setItem(newRow, 2, new QTableWidgetItem("0.0"));
}

void MainWindow::on_removeItemButton_clicked()
{
    QList<QTableWidgetSelectionRange> selectedRanges = ui->ItemsTable->selectedRanges();
    for (const QTableWidgetSelectionRange &range : selectedRanges) {
        for (int row = range.bottomRow(); row >= range.topRow(); --row) {
            ui->ItemsTable->removeRow(row);
        }
    }
}

void MainWindow::on_SaveInoviceButton_clicked()
{
    QString customer = ui->CustomerLineEdit->text().trimmed();
    QDate date = ui->dateEdit->date();

    if (customer.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Customer name cannot be empty.");
        return;
    }

    int rowCount = ui->ItemsTable->rowCount();
    if (rowCount == 0) {
        QMessageBox::warning(this, "Input Error", "Add at least one item to the invoice.");
        return;
    }

    QList<QList<QString>> items;

    for (int i = 0; i < rowCount; ++i) {
        QTableWidgetItem *itemNameItem = ui->ItemsTable->item(i, 0);
        QTableWidgetItem *quantityItem = ui->ItemsTable->item(i, 1);
        QTableWidgetItem *priceItem = ui->ItemsTable->item(i, 2);

        if (!itemNameItem || itemNameItem->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Input Error", QString("Item name in row %1 is empty.").arg(i + 1));
            return;
        }

        bool quantityOk;
        int quantity = quantityItem ? quantityItem->text().toInt(&quantityOk) : 0;
        if (!quantityOk || quantity <= 0) {
            QMessageBox::warning(this, "Input Error", QString("Quantity in row %1 is invalid.").arg(i + 1));
            return;
        }

        bool priceOk;
        double price = priceItem ? priceItem->text().toDouble(&priceOk) : 0.0;
        if (!priceOk || price < 0) {
            QMessageBox::warning(this, "Input Error", QString("Price in row %1 is invalid.").arg(i + 1));
            return;
        }

        items.append({itemNameItem->text().trimmed(), QString::number(quantity), QString::number(price)});
    }

    if (!db.transaction()) {
        QMessageBox::critical(this, "Database Error", "Failed to start transaction: " + db.lastError().text());
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO invoices (customer_name, date) VALUES (?, ?)");
    query.addBindValue(customer);
    query.addBindValue(date.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        db.rollback();
        QMessageBox::critical(this, "Database Error", "Failed to insert invoice: " + query.lastError().text());
        return;
    }

    qint64 invoiceId = query.lastInsertId().toLongLong();

    query.prepare("INSERT INTO invoice_items (invoice_id, item_name, quantity, price) VALUES (?, ?, ?, ?)");
    for (const QList<QString> &item : items) {
        query.addBindValue(invoiceId);
        query.addBindValue(item.at(0));
        query.addBindValue(item.at(1).toInt());
        query.addBindValue(item.at(2).toDouble());

        if (!query.exec()) {
            db.rollback();
            QMessageBox::critical(this, "Database Error", "Failed to insert invoice item: " + query.lastError().text());
            return;
        }
    }

    if (!db.commit()) {
        db.rollback();
        QMessageBox::critical(this, "Database Error", "Failed to commit transaction: " + db.lastError().text());
        return;
    }

    QMessageBox::information(this, "Success", "Invoice saved successfully!");

    // Reset form
    ui->CustomerLineEdit->clear();
    ui->dateEdit->setDate(QDate::currentDate());
    ui->ItemsTable->setRowCount(0);
}

void MainWindow::on_loadLastInvoice_clicked()
{
    QSqlQuery query;

    if (!query.exec("SELECT id, customer_name, date FROM invoices ORDER BY id DESC LIMIT 1")) {
        qDebug() << "Failed to fetch last invoice:" << query.lastError().text();
        return;
    }

    if (!query.next()) {
        // No invoice found, clear UI fields
        ui->CustomerLineEdit->clear();
        ui->dateEdit->setDate(QDate::currentDate());
        ui->ItemsTable->setRowCount(0);
        return;
    }

    int invoiceId = query.value(0).toInt();
    QString customerName = query.value(1).toString();
    QDate invoiceDate = QDate::fromString(query.value(2).toString(), "yyyy-MM-dd");

    ui->CustomerLineEdit->setText(customerName);
    ui->dateEdit->setDate(invoiceDate);
    ui->ItemsTable->setRowCount(0);

    QSqlQuery itemsQuery;
    itemsQuery.prepare("SELECT item_name, quantity, price FROM invoice_items WHERE invoice_id = ?");
    itemsQuery.addBindValue(invoiceId);
    if (!itemsQuery.exec()) {
        qDebug() << "Failed to fetch invoice items:" << itemsQuery.lastError().text();
        return;
    }

    while (itemsQuery.next()) {
        int newRow = ui->ItemsTable->rowCount();
        ui->ItemsTable->insertRow(newRow);

        ui->ItemsTable->setItem(newRow, 0, new QTableWidgetItem(itemsQuery.value(0).toString()));
        ui->ItemsTable->setItem(newRow, 1, new QTableWidgetItem(itemsQuery.value(1).toString()));
        ui->ItemsTable->setItem(newRow, 2, new QTableWidgetItem(itemsQuery.value(2).toString()));
    }
}
