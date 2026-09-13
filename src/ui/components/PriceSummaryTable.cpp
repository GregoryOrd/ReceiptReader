#include "PriceSummaryTable.h"
#include "common/Item.h"

#include "ui/components/NumericTableWidgetItem.h"
#include <QMessageBox>

const int NUM_COLS = 12;

const int CODE_COL = 0;
const int DESC_COL = 1;
const int CUR_PRICE_COL = 2;
const int MIN_PRICE_COL = 3;
const int MAX_PRICE_COL = 4;
const int DATE_COL = 5;
const int TIMES_PURCHASED_COL = 6;
const int LIFETIME_INFLATION_COL = 7;
const int ONE_YEAR_INFLATION_COL = 8;
const int SIX_MONTH_INFLATION_COL = 9;
const int THREE_MONTH_INFLATION_COL = 10;
const int ONE_MONTH_INFLATION_COL = 11;

PriceSummaryTable::PriceSummaryTable(ServerClient* serverClient) 
    : QTableWidget(nullptr), m_serverClient(serverClient)
{
    setColumnCount(NUM_COLS);

    _codeHdr = std::make_unique<QTableWidgetItem>("Code");
    _descHdr = std::make_unique<QTableWidgetItem>("Description");
    _currentPriceHdr = std::make_unique<QTableWidgetItem>("Current Price");
    _minPriceHdr = std::make_unique<QTableWidgetItem>("Min Price");
    _maxPriceHdr = std::make_unique<QTableWidgetItem>("Max Price");
    _dateHdr = std::make_unique<QTableWidgetItem>("Date");
    _lifetimeHeader = std::make_unique<QTableWidgetItem>("Lifetime Inflation Rate");
    _oneYearHdr = std::make_unique<QTableWidgetItem>("One Year Inflation Rate");
    _sixMnthHdr = std::make_unique<QTableWidgetItem>("6 Month Inflation Rate");
    _threeMnthHdr = std::make_unique<QTableWidgetItem>("3 Month Inflation Rate");
    _oneMnthHdr = std::make_unique<QTableWidgetItem>("1 Month Inflation Rate");
    _timesPurchasedHdr = std::make_unique<QTableWidgetItem>("Time Purchased");

    setHorizontalHeaderItem(CODE_COL, _codeHdr.get());
    setHorizontalHeaderItem(DESC_COL, _descHdr.get());
    setHorizontalHeaderItem(CUR_PRICE_COL, _currentPriceHdr.get());
    setHorizontalHeaderItem(MIN_PRICE_COL, _minPriceHdr.get());
    setHorizontalHeaderItem(MAX_PRICE_COL, _maxPriceHdr.get());
    setHorizontalHeaderItem(DATE_COL, _dateHdr.get());
    setHorizontalHeaderItem(LIFETIME_INFLATION_COL, _lifetimeHeader.get());
    setHorizontalHeaderItem(ONE_YEAR_INFLATION_COL, _oneYearHdr.get());
    setHorizontalHeaderItem(SIX_MONTH_INFLATION_COL, _sixMnthHdr.get());
    setHorizontalHeaderItem(THREE_MONTH_INFLATION_COL, _threeMnthHdr.get());
    setHorizontalHeaderItem(ONE_MONTH_INFLATION_COL, _oneMnthHdr.get());
    setHorizontalHeaderItem(TIMES_PURCHASED_COL, _timesPurchasedHdr.get());

    setColumnWidth(CODE_COL, 150);
    setColumnWidth(DESC_COL, 150);
    setColumnWidth(CUR_PRICE_COL, 150);
    setColumnWidth(MIN_PRICE_COL, 150);
    setColumnWidth(MAX_PRICE_COL, 150);
    setColumnWidth(DATE_COL, 150);
    setColumnWidth(LIFETIME_INFLATION_COL, 250);
    setColumnWidth(ONE_YEAR_INFLATION_COL, 250);
    setColumnWidth(SIX_MONTH_INFLATION_COL, 250);
    setColumnWidth(THREE_MONTH_INFLATION_COL, 250);
    setColumnWidth(ONE_MONTH_INFLATION_COL, 250);
    setColumnWidth(TIMES_PURCHASED_COL, 150);

    setSortingEnabled(true);
};

void PriceSummaryTable::mouseDoubleClickEvent(QMouseEvent* event) {
    if(event->button() == Qt::LeftButton){
        graphSelected();
        emit doubleClicked();
    }
    QTableWidget::mouseDoubleClickEvent(event); // Call base for default behaviour
}

void PriceSummaryTable::setChartView(PriceChart* chartView)
{
    m_chartView = chartView;
}

void PriceSummaryTable::graphSelected() {
    int row = currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select an item to graph.");
        return;
    }

    QString code = item(row, CODE_COL)->text();

    std::vector<Item> items;
    std::string error;
    if (!m_serverClient->queryItemCode(
            code.toStdString(),
            items,
            error)) {
        QMessageBox::warning(this, "Query Failed", QString::fromStdString(error));
        return;
    }

    if (items.empty()) {
        return;
    }

    m_chartView->plotItems(items, code);
}

void PriceSummaryTable::setItems(std::vector<PriceHistorySummary> priceSummaries)
{
    setRowCount(priceSummaries.size());
    for (int i = 0; i < priceSummaries.size(); i++) {
        const PriceHistorySummary ps = priceSummaries[i];

        setItem(i, CODE_COL, new QTableWidgetItem(QString::fromStdString(ps._code)));
        setItem(i, DESC_COL, new QTableWidgetItem(QString::fromStdString(ps._description)));
        setItem(i, CUR_PRICE_COL, new NumericTableWidgetItem(ps._currentPrice));
        setItem(i, MIN_PRICE_COL, new NumericTableWidgetItem(ps._minPrice));
        setItem(i, MAX_PRICE_COL, new NumericTableWidgetItem(ps._maxPrice));
        setItem(i, DATE_COL, new QTableWidgetItem(QString::fromStdString(ps._timestampLast)));
        setItem(i, LIFETIME_INFLATION_COL, new NumericTableWidgetItem(ps._liftimeInflationRate));
        setItem(i, ONE_YEAR_INFLATION_COL, new NumericTableWidgetItem(ps._oneYearInflationRate));
        setItem(i, SIX_MONTH_INFLATION_COL, new NumericTableWidgetItem(ps._sixMonthInflationRate));
        setItem(i, THREE_MONTH_INFLATION_COL, new NumericTableWidgetItem(ps._threeMonthInflationRate));
        setItem(i, ONE_MONTH_INFLATION_COL, new NumericTableWidgetItem(ps._oneMonthInflationRate));
        setItem(i, TIMES_PURCHASED_COL, new NumericTableWidgetItem(ps._timesPurchased));

        colourInflationRate(i, LIFETIME_INFLATION_COL, ps._liftimeInflationRate);
        colourInflationRate(i, ONE_YEAR_INFLATION_COL, ps._oneYearInflationRate);
        colourInflationRate(i, SIX_MONTH_INFLATION_COL, ps._sixMonthInflationRate);
        colourInflationRate(i, THREE_MONTH_INFLATION_COL, ps._threeMonthInflationRate);
        colourInflationRate(i, ONE_MONTH_INFLATION_COL, ps._oneMonthInflationRate);
    }

}

void PriceSummaryTable::colourCell(int row, int col, QColor color)
{
    QTableWidgetItem* cellItem = item(row, col);
    if (!cellItem) {
        return;
    }

    cellItem->setBackground(QBrush(color));
}

void PriceSummaryTable::colourInflationRate(int row, int col, double inflationRate)
{
    QColor color;
    if(inflationRate <= 2.0) {
        color = QColor(Qt::green);
    } else if(inflationRate <= 4.0) {
        color = QColor(Qt::yellow);
    } else {
        color = QColor(Qt::red);
    }

    colourCell(row, col, color);    
}