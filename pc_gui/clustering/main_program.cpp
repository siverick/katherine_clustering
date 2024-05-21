
/**
 * @main_program.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include "stdafx.h"
#include "main_program.h"
#include <QFileDialog>
#include <QDir>

const double ZoomInFactor = 0.8;
const double ZoomOutFactor = 1 / ZoomInFactor;

main_program::main_program(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    curScale = 1;
    
    /* Init GUI component values */
    ui.clusterSpan_input->setText("200");
    ui.clusterTime_input->setText("200");
    ui.filterSize_input->setText("0");
    ui.progressBar->setValue(0);
    ui.nextCLuster_edit->setText("0");
    ui.server_port_input->setText("21000");
    ui.server_mode_display->setText("offline");
    ui.server_status_display->setText("not running");

    // Create objects
    m_worker = new main_worker;
    render_thread = new QThread();
    m_worker->moveToThread(render_thread);
    render_thread->setObjectName("RenderThread");
    m_worker->setObjectName("Worker");

    // Connect GUI components with actions
    QObject::connect(m_worker, &main_worker::render_all, this, &main_program::update_pixmap);
    QObject::connect(ui.process_button, &QPushButton::clicked, m_worker, &main_worker::start_offline_clustering);
    QObject::connect(ui.processOnlineFile_button, &QPushButton::clicked, m_worker, &main_worker::start_online_file_clustering);
    QObject::connect(ui.save_clusters_button, &QPushButton::clicked, this, &main_program::save_done_clusters);
    QObject::connect(ui.kev_checkbox, &QCheckBox::stateChanged, m_worker, &main_worker::set_calib_state);
    QObject::connect(render_thread, &QThread::finished, render_thread, &QObject::deleteLater);
    QObject::connect(ui.nextCluster_button, &QPushButton::clicked, m_worker, &main_worker::next_cluster);
    QObject::connect(ui.prevCluster_button, &QPushButton::clicked, m_worker, &main_worker::prev_cluster);
    QObject::connect(ui.cluster_frame_button, &QPushButton::clicked, m_worker, &main_worker::toggle_cluster_frame);
    QObject::connect(ui.sendVariablesOnlineButton, &QPushButton::clicked, m_worker, &main_worker::sendParamsOnline);
    QObject::connect(ui.nextCLuster_edit, &QLineEdit::returnPressed, this, &main_program::show_specific_cluster);
    QObject::connect(ui.histogram_button, &QPushButton::clicked, m_worker, &main_worker::toggle_histogram);
    QObject::connect(ui.selectFile_button, &QPushButton::clicked, this, [this] {select_file_onclick(FileType::inputData); });
    QObject::connect(ui.selectCalib_A, &QPushButton::clicked, this, [this] {select_file_onclick(FileType::calibA); });
    QObject::connect(ui.selectCalib_B, &QPushButton::clicked, this, [this] {select_file_onclick(FileType::calibB); });
    QObject::connect(ui.selectCalib_C, &QPushButton::clicked, this, [this] {select_file_onclick(FileType::calibC); });
    QObject::connect(ui.selectCalib_T, &QPushButton::clicked, this, [this] {select_file_onclick(FileType::calibT); });
    QObject::connect(ui.setClusterSpan_button, &QPushButton::clicked, this, &main_program::set_cluster_span);
    QObject::connect(ui.setClusterTime_button, &QPushButton::clicked, this, &main_program::set_cluster_delay);
    QObject::connect(ui.setClusterSize_button, &QPushButton::clicked, this, &main_program::set_cluster_size);
    QObject::connect(ui.setFilterSize_button, &QPushButton::clicked, this, &main_program::set_filter_size);
    QObject::connect(m_worker, &main_worker::render_one, this, &main_program::show_cluster);
    QObject::connect(m_worker, &main_worker::cluster_info, this, &main_program::show_cluster_info);
    QObject::connect(m_worker, &main_worker::show_clustering_stats, this, &main_program::show_clustering_stats);
    QObject::connect(m_worker, &main_worker::update_progress, this, &main_program::update_progress);
    QObject::connect(this, &main_program::send_file_path, m_worker, &main_worker::load_file_path);
    QObject::connect(m_worker, &main_worker::show_histogram_online, this, &main_program::show_histogram_online);
    QObject::connect(m_worker, &main_worker::update_calib_status, this, &main_program::set_calib_status);
    QObject::connect(m_worker, &main_worker::show_popup, this, &main_program::show_popup);

    // Sorting data events
    QObject::connect(ui.sortBig_button, &QPushButton::clicked, m_worker, &main_worker::sortClustersBig);
    QObject::connect(ui.sortSmall_button, &QPushButton::clicked, m_worker, &main_worker::sortClustersSmall);
    QObject::connect(ui.sortToA_new_button, &QPushButton::clicked, m_worker, &main_worker::sortClustersNew);
    QObject::connect(ui.sortToa_old_button, &QPushButton::clicked, m_worker, &main_worker::sortClustersOld);
    QObject::connect(ui.clearData_button, &QPushButton::clicked, m_worker, &main_worker::clearClusters);

    /* Initialize server and online clustering stuff stuff */
    QObject::connect(ui.server_connect_button, &QPushButton::clicked, this, &main_program::start_online_clustering);
    QObject::connect(ui.server_idle_button, &QPushButton::clicked, this, [this] { select_online_plugin(plugins::idle); });
    QObject::connect(ui.server_histo_button, &QPushButton::clicked, this, [this] { select_online_plugin(plugins::clustering_energies); });
    QObject::connect(ui.server_pixelcounting_button, &QPushButton::clicked, this, [this] { select_online_plugin(plugins::pixel_counting); });
    QObject::connect(ui.server_clustering_button, &QPushButton::clicked, this, [this] { select_online_plugin(plugins::clustering_clusters); });
    QObject::connect(ui.server_receive_button, &QPushButton::clicked, this, [this] { select_online_plugin(plugins::simple_receiver); });
    QObject::connect(m_worker, &main_worker::server_mode_now, this, &main_program::display_mode);
    QObject::connect(m_worker, &main_worker::server_status_now, this, &main_program::display_stat);
    QObject::connect(m_worker, &main_worker::server_log_now, this, &main_program::display_log);

    /* Online stats*/
    QObject::connect(ui.stats_reset_screen_seconds, &QLineEdit::editingFinished, this, &main_program::update_reset_period);
    QObject::connect(ui.stats_reset_screen_enable, &QCheckBox::stateChanged, this, &main_program::update_reset_period);
    QObject::connect(ui.stats_reset_screen_enable, &QCheckBox::stateChanged, this, &main_program::enable_reset_screen);
    QObject::connect(m_worker, &main_worker::clusters_per_second, this, &main_program::update_stat_clusters_per_second);
    QObject::connect(m_worker, &main_worker::max_clusters_per_second, this, &main_program::update_stat_clusters_per_second_max);
    QObject::connect(m_worker, &main_worker::hitrate, this, &main_program::update_stat_hitrate);
    QObject::connect(m_worker, &main_worker::max_hitrate, this, &main_program::update_stat_hitrate_max);
    QObject::connect(m_worker, &main_worker::pixels_received, this, &main_program::update_stat_pixels_received);
    QObject::connect(m_worker, &main_worker::clusters_received, this, &main_program::update_stat_clusters_received);

    render_thread->start();
    render_thread->setPriority(QThread::HighestPriority);
}

main_program::~main_program()
{
    /*Once more quit if needed*/
    m_worker->abort = true;
    if (render_thread->isRunning())  render_thread->quit();
    if (render_thread->wait(5000) == false) {
        qWarning("Thread deadlock detected, bad things may happen !!!");
        render_thread->terminate();
        render_thread->wait();
    }

    delete m_worker;
    delete render_thread;
}

void main_program::update_pixmap(QPixmap image)
{
    ui.plot->setPixmap(image.scaled(ui.plot->width(), ui.plot->height(), Qt::KeepAspectRatio));
}

void main_program::show_cluster(QPixmap image, int longestSide) 
{
    ui.clusterPlot->setPixmap(image.scaled(256, 256, Qt::KeepAspectRatio));
}

void main_program::start_clustering()
{
    QObject::connect(render_thread, &QThread::started, m_worker, &main_worker::next_cluster);
    render_thread->start();
}

void main_program::show_cluster_info(int idx, int size, int total_ToT, int time_span, uint64_t ToA)
{
    if (time_span == 0) time_span = 1;

    QString txt = "ToA = " + QString::number(ToA) + " ns";
    txt.append ("\nToT =   " + QString::number(total_ToT));
    txt.append("\nTime Span = " + QString::number(time_span));
    txt.append(" ns");
    txt.append ("\nSize = " + QString::number(size) + " pix");

    ui.nextCLuster_edit->setText(QString::number(idx));
    ui.textBrowser->setFontPointSize(10);
    ui.textBrowser->setText(txt);
}

void main_program::show_clustering_stats(std::string stats)
{
    ui.text_stats_browser->append(QString::fromStdString(stats));
}

void main_program::update_progress(int val)
{
    ui.progressBar->setValue(val);
}

void main_program::select_file_onclick(FileType type)
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Open s file", QDir::homePath());
    switch (type) {
    case FileType::inputData:
        ui.file_label->setText(fileName);
        break;
    case FileType::calibA:
        ui.calibA_label->setText(fileName);
        break;
    case FileType::calibB:
        ui.calibB_label->setText(fileName);
        break;
    case FileType::calibC:
        ui.calibC_label->setText(fileName);
        break;
    case FileType::calibT:
        ui.calibT_label->setText(fileName);
        break;
    }
    //m_worker->load_file_path(fileName);
    emit send_file_path(fileName, type);
}

void main_program::set_cluster_span()
{
    m_worker->set_cluster_span(ui.clusterSpan_input->text().toInt());
}

void main_program::set_cluster_delay()
{
    m_worker->set_cluster_delay(ui.clusterTime_input->text().toInt());
}

void main_program::set_cluster_size()
{
    bool filterBiggerCheck = ui.clusterSize_checkbox->isChecked();
    int filterValue = ui.clusterSize_input->text().toInt();

    if (filterValue < 0)
    {
        emit show_popup("Cluster Size filter set fail.", "Please input positive number.", QMessageBox::Warning);
        ui.clusterSize_input->setText("");
        return;
    }
    if (filterValue > 65000)
    {
        emit show_popup("Cluster Size filter set fail.", "Please input number smaller than 65000", QMessageBox::Warning);
        ui.clusterSize_input->setText("");
        return;
    }

    m_worker->set_cluster_size(filterValue, filterBiggerCheck);
}

void main_program::set_filter_size()
{
    int filterValue = ui.filterSize_input->text().toInt();

    if (filterValue < 0)
    {
        emit show_popup("Filter set fail.", "Please input positive number.", QMessageBox::Warning);
        ui.clusterSize_input->setText("");
        return;
    }
    if (filterValue > 100)
    {
        emit show_popup("Filter set fail.", "Please input number smaller than 100", QMessageBox::Warning);
        ui.clusterSize_input->setText("");
        return;
    }

    auto value = ui.filterSize_input->text().toFloat();     // toInt() wouldnt work if user inputs float
    m_worker->set_filter_size(static_cast<int>(value));
}

void main_program::show_specific_cluster()
{
    QString index = ui.nextCLuster_edit->text();

    bool result = m_worker->specific_cluster(index.toInt());
    if (result == false)
    {
        ui.nextCLuster_edit->setText("err");
    }
}

/* display calibration status in form of label color of given calibration file */
void main_program::set_calib_status(bool succesful, FileType type)
{
    switch (type)
    {
    case FileType::calibA:
        if(succesful) ui.calibA_label->setStyleSheet("QLabel { background-color : green; color : black; }");
        else ui.calibA_label->setStyleSheet("QLabel { background-color : red; color : blue; }");
        break;
    case FileType::calibB:
        if (succesful) ui.calibB_label->setStyleSheet("QLabel { background-color : green; color : black; }");
        else ui.calibB_label->setStyleSheet("QLabel { background-color : red; color : blue; }");
        break;
    case FileType::calibC:
        if (succesful) ui.calibC_label->setStyleSheet("QLabel { background-color : green; color : black; }");
        else ui.calibC_label->setStyleSheet("QLabel { background-color : red; color : blue; }");
        break;
    case FileType::calibT:
        if (succesful) ui.calibT_label->setStyleSheet("QLabel { background-color : green; color : black; }");
        else ui.calibT_label->setStyleSheet("QLabel { background-color : red; color : blue; }");
        break;
    default:    // Only input file is left - but dont signal
        break;
    }
}

void main_program::init_histogram()
{
    const int winX = 1600;
    const int winY = 900;
    kev_histo = new QBarSet("Energy histogram (kEV)");

    ser = new QBarSeries();
    ser->append(kev_histo);
    ser->setBarWidth(1);

    chart = new QChart();
    QMargins margins(10, 10, 10, 10);
    chart->setMargins(margins); // Set margins around plot area
    chart->addSeries(ser);
    chart->setTitle("Energy histogram");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setTheme(QChart::ChartThemeLight);
    chart->setAnimationDuration(0);

    axisx = new QValueAxis;
    axisx->setTitleText("ToT / Energy (kEV)");
    axisx->setTickCount(winX / 40);
    axisx->setLabelFormat("%d");
    axisx->setMinorGridLineVisible();
    chart->addAxis(axisx, Qt::AlignBottom);
    ser->attachAxis(axisx);

    axisy = new QValueAxis;
    axisy->setTitleText("Count");
    axisy->setTickCount(winY / 40);
    axisy->setLabelFormat("%d");
    axisy->setMinorGridLineVisible();
    chart->addAxis(axisy, Qt::AlignLeft);
    ser->attachAxis(axisy);

    chartView = new QChartView();
    chartView->setRubberBand(QChartView::HorizontalRubberBand);
    chartView->setAlignment(Qt::AlignCenter);
    chartView->resize(winX, winY);
    chartView->setChart(chart);
    //chartView->setRenderHint(QPainter::Antialiasing);
    histogram_inited = true;
}

void main_program::show_histogram_online(std::vector<uint16_t> histo, bool reset_required)
{
    // Variables for determining range
    static int maxY = 0;
    static int maxX = 0;
    static int minX = 10000;

    if (histogram_inited == false)
    {
        init_histogram();
    }
    
    // reset QBarSeries
    if (reset_required == true)
    {
        maxX = 0;
        maxY = 0;
        minX = 10000;
        kev_histo->remove(0, kev_histo->count());
    }
    
    if (histo.empty()) return;

    // make sure we have enough bars
    while (histo.size() > kev_histo->count())
    {
        *kev_histo << 0;
    }

    int i = 0;
    for (auto& bar : histo)
    {
        if (bar > 0)
        {
            //*kev_histo << bar;
            uint32_t count = (bar + kev_histo->at(i));
            kev_histo->replace(i, count);    // Replace value by old_value + new value

            if (maxY < count) {
                maxY = count;
            }

            if (i > maxX)
                maxX = i;

            if (i < minX)
            {
                minX = i;
            }
        }

        i++;
    }

    // Update axis ranges
    chart->axisX()->setRange(minX-2, round(maxX * 1.02));
    chart->axisY()->setRange(0, maxY);

    // Dynamic range update - keep bars visible
    int dynamicRange = maxX - minX;
    ser->setBarWidth((dynamicRange < 400) ? 1 : ceil(static_cast<float>(dynamicRange)/400.0f));
    
    // Clip X count to meaningful value - so there is not too much numbers
    axisx->setTickCount((maxX < (1600 / 40)) ? (maxX + 1) : (1600 / 40));
    // Clip Y tick count to 50 - then becomes overfilled
    axisy->setTickCount((maxY < 10) ? (maxY + 1) : 10);

    // Show chart if user has hidden it
    if (chartView->isVisible() == false)
    {
        chartView->setAlignment(Qt::AlignCenter);
        chartView->resize(1600, 900);
        chartView->show();
    }
}

void main_program::show_popup(QString title, QString message, QMessageBox::Icon type)
{
    QMessageBox msgBox;
    msgBox.setText(message);
    msgBox.setWindowTitle(title);
    msgBox.setIcon(type);
    msgBox.exec();
}

void main_program::zoom(double zoomFactor)
{
    curScale *= zoomFactor;
    update();
    //thread.render_all(centerX, centerY, curScale, size(), devicePixelRatio());

    /* Redraw the image */

}

#if QT_CONFIG(wheelevent)
//! [12]
void main_program::wheelEvent(QWheelEvent* event)
{
    static int wheels = 0;
    wheels++;
    QString numOfWheels = QString::number(wheels);

    const int numDegrees = event->angleDelta().y() / 8;
    const double numSteps = numDegrees / double(15);
    zoom(pow(ZoomInFactor, numSteps));

    //ui.label->setText("Wheel no: " + numOfWheels);
}
//! [12]
#endif

//--------------------------
/* Online clustering stuff */
//--------------------------

void main_program::start_online_clustering()
{
    static bool connected = false;

    int32_t port = static_cast<int32_t>(ui.server_port_input->text().toInt());

    // Disconnect
    if (connected == true)
    {
        connected = false;
        m_worker->toggle_online_clustering(port);
        ui.server_connect_button->setText("Connect");
        return;
    }

    if (port < 0 || port > 65535) 
    {
        popup_warning("Port warning!", "Please set port to number between 0 and 65535.");
        ui.server_port_input->setText("0");
    }
    // Connect
    else if (connected == false)
    {
        connected = true;
        m_worker->toggle_online_clustering(port);
        ui.server_connect_button->setText("Disconnect");
    }

}

void main_program::select_online_plugin(plugins mode)
{
    m_worker->select_online_mode(mode);
}

void main_program::update_stat_hitrate(uint64_t val)
{
    QString unit;
    float hitrate = 0;
    if (val > 100000)
    {
        hitrate = static_cast<float>(val) / 1000000;
        unit = " MHit/s";
        unit.prepend(QString::number(hitrate, 'f', 2));
    }
    else if (val > 100)
    {
        hitrate = static_cast<float>(val) / 1000;
        unit = " kHit/s";
        unit.prepend(QString::number(hitrate, 'f', 2));
    }
    else 
    {
        hitrate = static_cast<float>(val);
        unit = " Hit/s";
        unit.prepend(QString::number(hitrate, 'f', 0));
    }

    ui.stats_hitrate->setText(unit);
}

void main_program::update_stat_hitrate_max(uint64_t val)
{
    QString unit;
    float hitrate = 0;
    if (val > 100000)
    {
        hitrate = static_cast<float>(val) / 1000000;
        unit = " MHit/s";
        unit.prepend(QString::number(hitrate, 'f', 2));
    }
    else if (val > 100)
    {
        hitrate = static_cast<float>(val) / 1000;
        unit = " kHit/s";
        unit.prepend(QString::number(hitrate, 'f', 2));
    }
    else
    {
        hitrate = static_cast<float>(val);
        unit = " Hit/s";
        unit.prepend(QString::number(hitrate, 'f', 0));
    }

    ui.stats_hitrate_max->setText(unit);
}

void main_program::update_stat_clusters_per_second(uint64_t val)
{
    QString unit;
    float cl_hitrate = 0;
    if (val > 100000)
    {
        cl_hitrate = static_cast<float>(val) / 1000000;
        unit = " MCl/s";
        unit.prepend(QString::number(cl_hitrate, 'f', 3));
    }
    else if (val > 100)
    {
        cl_hitrate = static_cast<float>(val) / 1000;
        unit = " kCl/s";
        unit.prepend(QString::number(cl_hitrate, 'f', 2));
    }
    else
    {
        cl_hitrate = static_cast<float>(val);
        unit = " Cl/s";
        unit.prepend(QString::number(cl_hitrate, 'f', 0));
    }

    ui.stats_clusters_per_second->setText(unit);
}

void main_program::update_stat_clusters_per_second_max(uint64_t val)
{
    QString unit;
    float cl_hitrate = 0;
    if (val > 100000)
    {
        cl_hitrate = static_cast<float>(val) / 1000000;
        unit = " MCl/s";
        unit.prepend(QString::number(cl_hitrate, 'f', 3));
    }
    else if (val > 100)
    {
        cl_hitrate = static_cast<float>(val) / 1000;
        unit = " kCl/s";
        unit.prepend(QString::number(cl_hitrate, 'f', 2));
    }
    else
    {
        cl_hitrate = static_cast<float>(val);
        unit = " Cl/s";
        unit.prepend(QString::number(cl_hitrate, 'f', 0));
    }

    ui.stats_clusters_per_second_max->setText(unit);
}

void main_program::update_stat_pixels_received(uint64_t val)
{
    QString unit;
    float pixels = 0;
    if (val > 1000000)
    {
        pixels = static_cast<float>(val) / 1000000;
        unit = " Mpixs";
        unit.prepend(QString::number(pixels, 'f', 3));
    }
    else if (val > 1000)
    {
        pixels = static_cast<float>(val) / 1000;
        unit = " Kpixs";
        unit.prepend(QString::number(pixels, 'f', 3));
    }
    else
    {
        pixels = static_cast<float>(val);
        unit = " pixs";
        unit.prepend(QString::number(pixels, 'f', 0));
    }

    ui.stats_pixels_received->setText(unit);
}

void main_program::update_stat_clusters_received(uint64_t val)
{
    QString unit;
    float clusters = 0;
    if (val > 1000000)
    {
        clusters = static_cast<float>(val) / 1000000;
        unit = " Mclus";
        unit.prepend(QString::number(clusters, 'f', 3));
    }
    else if (val > 1000)
    {
        clusters = static_cast<float>(val) / 1000;
        unit = " Kclus";
        unit.prepend(QString::number(clusters, 'f', 3));
    }
    else
    {
        clusters = static_cast<float>(val);
        unit = " clus";
        unit.prepend(QString::number(clusters, 'f', 0));
    }

    ui.stats_clusters_received->setText(unit);
}

void main_program::enable_reset_screen()
{
    bool enabled = ui.stats_reset_screen_enable->isChecked();
    m_worker->enable_reset_screen(enabled);
}

void main_program::update_reset_period()
{
    int per = ui.stats_reset_screen_seconds->text().toInt();
    if (per < 200) 
    {
        popup_error("Reset period out of range!", "Please input period longer than 200 ms.");
        ui.stats_reset_screen_seconds->setText("200");
    }
    m_worker->update_reset_period(per);
}

void main_program::display_mode(plugins plug)
{
    switch (plug)
    {
    case plugins::idle:
        ui.server_mode_display->setText("idle - not receiving");
        break;
    case plugins::clustering_clusters:
        ui.server_mode_display->setText("receiving clusters");
        break;
    case plugins::simple_receiver:
        ui.server_mode_display->setText("receiving pixels");
        break;
    case plugins::clustering_energies:
        ui.server_mode_display->setText("receiving energies(histogram)");
        break;
    case plugins::pixel_counting:
        ui.server_mode_display->setText("pixel counting");
        break;
    default:
        ui.server_mode_display->setText("no mode");
        break;
    }
}

void main_program::display_stat(plugin_status stat)
{
    switch (stat)
    {
    case plugin_status::loading:
        ui.server_status_display->setText("connecting");
        ui.server_label->setStyleSheet("QLabel { background-color : blue; color : white; }");
        break;
    case plugin_status::loading_error:
        ui.server_status_display->setText("connection error");
        ui.server_label->setStyleSheet("QLabel { background-color : red; color : blue; }");
        break;
    case plugin_status::ready:
        ui.server_status_display->setText("ready");
        ui.server_label->setStyleSheet("QLabel { background-color : green; color : black; }");
        break;
    case plugin_status::running:
        ui.server_status_display->setText("running");
        ui.server_label->setStyleSheet("QLabel { background-color : green; color : black; }");
        break;
    default:
        ui.server_status_display->setText("disabled");
        ui.server_label->setStyleSheet("QLabel { background-color : red; color : blue; }");
        break;
    }
}

void main_program::display_log(std::string message)
{
    ui.server_log_browser->append(QString::fromStdString(message));
}

void main_program::save_done_clusters()
{
    // Diplay warning if no clusters can be saved
    if (m_worker->get_number_of_clusters() < 1)
    {
        popup_warning("Saving failed!", "Saving failed: No data to be saved");
        return;
    }
    
    auto call = [&]() { m_worker->save_done_clusters(); };
    std::thread saving = std::thread(call);
    update_progress(0);

    // Show progress
    int progress = 0;
    while (m_worker->doneSaving == false)
    {
        progress += 1;
        if (progress > 100) progress = 1;
        update_progress(progress);

        QCoreApplication::processEvents(QEventLoop::ProcessEventsFlag::AllEvents, 100); // Keep window responsive
        QThread::msleep(20);
    }

    update_progress(100);
    m_worker->doneSaving = false;
    saving.join();

    // Show success message
    popup_info("Saving successful!", "Saved to folder: ../clustering/saved_pixel_data");
}
