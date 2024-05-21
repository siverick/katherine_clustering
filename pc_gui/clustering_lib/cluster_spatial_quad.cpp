#include "cluster_quadtree.h"

uint8_t clustering_spatial_quadtree::QuadTree::depth = 0;
clustering_spatial_quadtree::Clusters clustering_spatial_quadtree::QuadTree::doneClusters;
ClusteringParams clustering_spatial_quadtree::QuadTree::params;

void clustering_spatial_quadtree::do_clustering(std::string& lines, const ClusteringParams& params, volatile bool& abort)
{
    if (lines == "") return;
    if (lines[0] != '#') return;
    stat_reset();

    // PARAMETERS
    int upFilter = 255 - params.outerFilterSize;
    int doFilter = params.outerFilterSize;
    double toaLsb = 25;
    double toaFineLsb = 1.5625;

    // Loop variables
    QuadTree tree(params);

    std::string oneLine;
    char* context = nullptr;
    char* rows[4] = { 0, 0, 0, 0 };

    ContinualTimer timer;
    timer.Start();
    while (get_my_line(lines, oneLine, params.rn_delim)) {      // Gets lines without ending line chars - ex. "\n"

        if (oneLine[0] == '#') continue;

        if (abort)
        {
            return;
        }

        stat_lines_sorted++;

        /* Split line into rows */
        rows[0] = strtok_s(&oneLine.front(), "\t", &context); //strtok(oneLine.data(), "\t");
        rows[1] = strtok_s(NULL, "\t", &context);
        rows[2] = strtok_s(NULL, "\t", &context);
        rows[3] = strtok_s(NULL, "\t", &context);

        // ATOI 6300 ms vs STRTOINT 6100 ms with 20 PIX filter
        // ATOI CCA 200 ms slower than my Fast strtoint
        int coordX = strtoint(rows[0]) / 256; // std::atoi(rows[0]) / 256;
        int coordY = strtoint(rows[0]) % 256; // std::atoi(rows[0]) % 256;
        double toaAbsTime = (double)((strtolong(rows[1]) * toaLsb) - (strtolong(rows[2]) * toaFineLsb));  // (ns) TimeFromBeginning = 25 * ToA - 1.5625 * fineToA
        int ToTValue = strtoint(rows[3]);//std::atoi(rows[3]);
        if (coordX > upFilter || coordX < doFilter || coordY > upFilter || coordY < doFilter) continue;   // This is faster after the inlined funkctions, not after coordY

        if (params.calibReady)
        {
            ToTValue = energy_calc(coordX, coordY, ToTValue);
        }

        tree.ProcessPixel(OnePixel{ (uint16_t)coordX, (uint16_t)coordY, ToTValue, toaAbsTime });
        stat_lines_processed++;
    }
    timer.Stop();

    /* POSTPROCESS Clusters */
    tree.GetDoneClusters(doneClusters);

    /* Utility functions after the clustering */
    test_saved_clusters();
    stat_save(timer.ElapsedMs(), doneClusters.size());
    return;
}

void clustering_spatial_quadtree::show_progress(bool newOp, int no_lines)
{
#define no_prog 100
#define inc (100/no_prog)
    static int prog = 0;
    static int last_prog = 0;
    static int prog_val = 0;

    /* Progress bar clutter */
    if (newOp == false)
    {
        prog++;
        if (prog - last_prog > no_lines / no_prog)
        {
            last_prog = prog;
            prog_val += inc;
            //emit obj.update_progress(prog_val);
        }
    }
    else
    {
        //emit obj.update_progress(0);
        prog = 0;
        last_prog = 0;
        prog_val = 0;
    }
}
