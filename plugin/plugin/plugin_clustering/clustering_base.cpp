/**
 * @clustering_base.cpp
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#include <clustering_base.h>

int clustering_base::test_all_lines_used() {
    assert("Fault = lines processed doesnt match" && stat_lines_processed == stat_lines_saved);

    if (stat_lines_processed == stat_lines_saved)
    {
        return 0;   // Ok  
    }
 
    return -1;  // Fault
}

bool clustering_base::get_my_line(const std::string& str, std::string& oneLine, const bool& rn_delim)
{
    static size_t pos = 0;
    static size_t last_pos = 0;
    pos = str.find('\n', pos + 1);

    if (rn_delim) oneLine = str.substr(last_pos, pos - last_pos - 1);
    else oneLine = str.substr(last_pos, pos - last_pos);

    // This was for fast_atoi fun
    /*int i = 1;
    while (oneLine[0] == ' ') {
        if (rn_delim) oneLine = str.substr(last_pos + i, pos - last_pos - 1 - i);
        else oneLine = str.substr(last_pos + i, pos - last_pos - i);
        i++;
    }*/

    last_pos = pos + 1;

    if (pos != std::string::npos) return true;
    else return false;
}

bool clustering_base::get_my_line_MT(const std::string& str, std::string& oneLine, const bool& rn_delim, size_t& last_pos)
{
    size_t pos = 0;
    pos = str.find('\n', pos + 1);

    if (rn_delim) oneLine = str.substr(last_pos, pos - last_pos - 1);
    else oneLine = str.substr(last_pos, pos - last_pos);

    // This was for fast_atoi fun
    /*int i = 1;
    while (oneLine[0] == ' ') {
        if (rn_delim) oneLine = str.substr(last_pos + i, pos - last_pos - 1 - i);
        else oneLine = str.substr(last_pos + i, pos - last_pos - i);
        i++;
    }*/

    last_pos = pos + 1;

    if (pos != std::string::npos) return true;
    else return false;
}

double clustering_base::energy_calc(const int& x, const int& y, const int& ToT)
{
    double temp_energy = 0;
    double a, b, c, t;
    size_t calib_matrix_offset = static_cast<size_t>(x) + (static_cast<size_t>(y) * 256);

    a = calib_a[calib_matrix_offset];
    b = calib_b[calib_matrix_offset];
    c = calib_c[calib_matrix_offset];
    t = calib_t[calib_matrix_offset];

    double tmp = std::pow(b + (t * a) - (double)ToT, 2) + (4 * a * c);

    if (tmp > 0)
    {
        tmp = std::sqrt(tmp);
        temp_energy = ((t * a) + (double)ToT - b + tmp) / (2.0 * a);
    }
    else
        temp_energy = 0;

    return temp_energy;
}

float clustering_base::perf_metric(int linesProcessed, float msElapsed)
{
    return (float)linesProcessed / (float)msElapsed / 1000.0f;
}

template <typename T>
T clustering_base::sort_clusters_generic(SortType type, T doneOnes)
{
    if (type == SortType::bigFirst)
    {
        std::sort(doneOnes.begin(), doneOnes.end(),
            [](T a, T b) {
                return a.pix.size() > b.pix.size();
            });
    }
    else if (type == smallFirst)
    {
        std::sort(doneOnes.begin(), doneOnes.end(),
            [](T a, T b) {
                return a.pix.size() < b.pix.size();
            });
    }

    return doneOnes;
}

void clustering_base::set_calib_a(double* calib)
{
    for (int i = 0; i < 256 * 256; i++) {
        calib_a[i] = *calib++;
    }
}

void clustering_base::set_calib_b(double* calib)
{
    for (int32_t i = 0; i < 256 * 256; i++) {
        calib_b[i] = *calib++;
    }
}

void clustering_base::set_calib_c(double* calib)
{
    for (int32_t i = 0; i < 256 * 256; i++) {
        calib_c[i] = *calib++;
    }
}

void clustering_base::set_calib_t(double* calib)
{
    for (int32_t i = 0; i < 256 * 256; i++) {
        calib_t[i] = *calib++;
    }
}

void clustering_base::set_calibs(double* a, double* b, double* c, double* t)
{
    for (int32_t i = 0; i < 256 * 256; i++)
    {
        calib_a[i] = *a++;
        calib_b[i] = *b++;
        calib_c[i] = *c++;
        calib_t[i] = *t++;
    }
}
