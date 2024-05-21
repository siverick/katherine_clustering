
/**
 * @main_program.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#include <QtWidgets/QWidget>
#include "ui_clustering.h"
#include <QGestureEvent>
#include "worker.h"
#include "plugin_definition.h"
#include <thread>

#ifndef __INTELLISENSE__
#include <QMutex>
#endif

class main_program : public QWidget, private plugin_definition
{
    Q_OBJECT

public:
    main_program(QWidget *parent = nullptr);
    ~main_program();

public slots:
    void zoom(double zoomFactor);
    void update_pixmap(QPixmap image);
    void show_cluster(QPixmap image, int longestSide);
    void start_clustering();
    void update_progress(int val);
    void show_cluster_info(int idx, int size, int total_ToT, int time_span, uint64_t ToA);
    void show_clustering_stats(std::string stats);
    void select_file_onclick(FileType type);
    void set_cluster_span();
    void set_cluster_delay();
    void set_cluster_size();
    void set_filter_size();
    void show_specific_cluster();
    void set_calib_status(bool succesful, FileType type);
    void show_histogram_online(std::vector<uint16_t> histo, bool reset_required);
    void show_popup(QString title, QString message, QMessageBox::Icon type);

    // Online stuff
    void start_online_clustering();
    void select_online_plugin(plugins mode);
    void update_stat_hitrate(uint64_t val);
    void update_stat_hitrate_max(uint64_t val);
    void update_stat_clusters_per_second(uint64_t val);
    void update_stat_clusters_per_second_max(uint64_t val);
    void update_stat_pixels_received(uint64_t val);
    void update_stat_clusters_received(uint64_t val);
    void enable_reset_screen();
    void update_reset_period();

    void display_mode(plugins plug);
    void display_stat(plugin_status stat);
    void display_log(std::string message);

    // Saving stuff
    void save_done_clusters();

signals:
    void send_file_path(QString path, FileType type);

private:
    Ui::clusteringClass ui;
    QPixmap picture;
    QPainter painter;
    double curScale;
    QThread* render_thread;
    main_worker* m_worker;

    // Histogram
    bool histogram_inited = false;
    QChartView* chartView = nullptr;
    QBarSet* kev_histo = nullptr;
    QBarSeries* ser = nullptr;
    QChart* chart = nullptr;
    QValueAxis* axisx = nullptr;
    QValueAxis* axisy = nullptr;
    void init_histogram();

    /* UI Utilities */
    void popup_info(QString title, QString message)
    {
        QMessageBox msgBox;
        msgBox.setText(message);
        msgBox.setWindowTitle(title);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    }

    void popup_warning(QString title, QString message)
    {
        QMessageBox msgBox;
        msgBox.setText(message);
        msgBox.setWindowTitle(title);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.exec();
    }

    void popup_error(QString title, QString message)
    {
        QMessageBox msgBox;
        msgBox.setText(message);
        msgBox.setWindowTitle(title);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
    }

protected:
#if QT_CONFIG(wheelevent)
    void wheelEvent(QWheelEvent* event) override;
#endif
};
