#ifndef CLUSTERER_BOUNDARIES_H_
#define CLUSTERER_BOUNDARIES_H_

/**
 * @file boundaries.h
 * @author Lukas Meduna (meduna@medunalukas.com)
 * @brief Class used to store 2D boundaries, used in clustering to denote boundaries of cluster.
 * @date 01/11/2018
 *
 * @copyright Lukas Meduna (c) 2018
 *
 */

template <typename T> class Boundaries {
public:
  Boundaries() = default;
  ~Boundaries() = default;
  Boundaries<T>(T xmin, T xmax, T ymin, T ymax)
      : x_min(xmin), x_max(xmax), y_min(ymin), y_max(ymax) {
    dimension = std::max(x_max - x_min, y_max - y_max);
  };

  /**
   * @brief Checks if x, y points lies inside boundary
   *
   * @param x
   * @param y
   * @return true
   * @return false
   */
  bool Hit(const T &x, const T &y) {
    if (x >= x_min && x < x_max && y >= y_min && y < y_max)
      return true;
    else
      return false;
  }
  bool Hit(const Boundaries &b) {
    if (b.x_min >= x_min && b.x_max <= x_max && b.y_min >= y_min && b.y_max <= y_max)
      return true;
    else
      return false;
  }
  friend std::ostream &operator<<(std::ostream &output, const Boundaries &b) {
    output << "Boundaries: x: min " << b.x_min << " max " << b.x_max << " ,y: min " << b.y_min
           << " max " << b.y_max << " dimension " << b.dimension;
    return output;
  }
  friend bool operator==(const Boundaries &l, const Boundaries &r) {
    if (l.x_min == r.x_min && l.x_max == r.x_max && l.y_min == r.y_min && l.y_max == r.y_max)
      return true;
    else
      return false; // keep the same order
  }
  friend bool operator!=(const Boundaries &l, const Boundaries &r) { return !(l == r); }

  T x_min;
  T x_max;
  T y_min;
  T y_max;
  T dimension;
};
#endif // CLUSTERER_BOUNDARIES_H_
