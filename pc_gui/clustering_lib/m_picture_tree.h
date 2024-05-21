#ifndef CLUSTERER_PICTURE_TREE_H_
#define CLUSTERER_PICTURE_TREE_H_
/**
 * @file picture_tree.h
 * @author Lukas Meduna (meduna@medunalukas.com)
 * @brief 2D quad tree used to keep track of neighbour pixels and fast search in them.
 * @date 01/11/2018
 *
 * @copyright Lukas Meduna (c) 2018
 *
 */

#include <algorithm>
#include <bitset>
#include <iostream>
#include <list>
#include <memory>

#include "m_boundaries.h"

/**
 * @brief Keeps points in quad tree structure.
 *
 * @tparam chip_size
 * @tparam leaf_size The smallest leaf size, rest is represented as 2D matrix
 */
template <int chip_size, int leaf_size> class PictureTree {
public:
  /**
   * @brief Checks if tree contains pixel
   *
   * @param x
   * @param y
   * @return true
   * @return false
   */
  bool Contains(const int &x, const int &y) {
    if (root_ != nullptr)
      // recursive call
      if (root_->boundaries_.Hit(x, y))
        return root_->Contains(x, y);
      else
        return false;
    else
      return false;
  }

  /**
   * @brief Add pixel to tree
   *
   * @param x
   * @param y
   */
  void AddPoint(const int &x, const int &y) {
    // If out of bound, skip.
    if (x < 0 || x > chip_size || y < 0 || y > chip_size)
      return;

    // If no root is present, create new
    if (root_ == nullptr) {
      int minX, minY;
      // Get nearest cell boundary (leaf size)
      minX = x - std::abs(x % leaf_size);
      minY = y - std::abs(y % leaf_size);
      root_ = std::make_unique<PictureTreeNode>(
          Boundaries<int>(minX, minX + leaf_size, minY, minY + leaf_size));
    }
    // Recursive code. If it belongs to the boundary (rectangle around) add it.
    if (root_->boundaries_.Hit(x, y)) {
      root_->AddPoint(x, y);
    } else {
      do {
        // Pixel is not in roots boundary, so we need to create new root and extend boundaries in
        // correct direction. Thus we find correct quater (direction) and create new root and bind
        // old root into correct branch.
        // ASCI painting of quaters
        // y_max
        //   +----+-----+
        //   |  0 |  1  |
        //.  +----+-----+
        //   |  2 |  3  |
        //   +----+-----+
        // y_min        x_max
        // x_min
        size_t quarter = root_->GetQuarter(x, y);
        std::unique_ptr<PictureTreeNode> new_root = nullptr;
        switch (quarter) {
        case 0:
          new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
              root_->boundaries_.x_min - root_->boundaries_.dimension, root_->boundaries_.x_max,
              root_->boundaries_.y_min, root_->boundaries_.y_max + root_->boundaries_.dimension));
          new_root->childern_[3] = std::move(root_);
          root_ = std::move(new_root);
          break;
        case 1:
          new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
              root_->boundaries_.x_min, root_->boundaries_.x_max + root_->boundaries_.dimension,
              root_->boundaries_.y_min, root_->boundaries_.y_max + root_->boundaries_.dimension));
          new_root->childern_[2] = std::move(root_);
          root_ = std::move(new_root);
          break;
        case 2:
          new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
              root_->boundaries_.x_min - root_->boundaries_.dimension, root_->boundaries_.x_max,
              root_->boundaries_.y_min - root_->boundaries_.dimension, root_->boundaries_.y_max));
          new_root->childern_[1] = std::move(root_);
          root_ = std::move(new_root);
          break;
        case 3:
          new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
              root_->boundaries_.x_min, root_->boundaries_.x_max + root_->boundaries_.dimension,
              root_->boundaries_.y_min - root_->boundaries_.dimension, root_->boundaries_.y_max));
          new_root->childern_[0] = std::move(root_);
          root_ = std::move(new_root);
          break;
        default:
          throw "Bad state(quarter)";
          break;
        }
      } while (!root_->boundaries_.Hit(x, y)); // Increase until pixel is in boundary of root
      // Tree cover place of pixel and we can finally add it.
      root_->AddPoint(x, y);
    }
  }
  /**
   * @brief Takes second tree and steals all its points. Second tree is than unspecified state,
   * should be deleted.
   *
   * @param pt Second tree to steal from.
   */
  void JoinWith(PictureTree &pt) {
    // Tell the second tree to prepare all its leafs
    pt.root_->PrepareToSteal(to_steal_);
    // One by one leaf add them to this tree
    for (auto c : to_steal_) {
      // If this tree boundary contains the leaf, just add it
      if (root_->boundaries_.Hit(c->boundaries_)) {
        root_->AddLeaf(c);
      } else {
        // If leaf is out of bounds, repeatly increase size until boundary is OK
        do {
          size_t quarter = root_->GetQuarter(c->boundaries_);
          // ASCI painting of quaters
          // y_max
          //   +----+-----+
          //   |  0 |  1  |
          //.  +----+-----+
          //   |  2 |  3  |
          //   +----+-----+
          // y_min        x_max
          // x_min
          std::unique_ptr<PictureTreeNode> new_root = nullptr;
          switch (quarter) {
          case 0:
            new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
                root_->boundaries_.x_min - root_->boundaries_.dimension, root_->boundaries_.x_max,
                root_->boundaries_.y_min, root_->boundaries_.y_max + root_->boundaries_.dimension));
            new_root->childern_[3] = std::move(root_);
            root_ = std::move(new_root);
            break;
          case 1:
            new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
                root_->boundaries_.x_min, root_->boundaries_.x_max + root_->boundaries_.dimension,
                root_->boundaries_.y_min, root_->boundaries_.y_max + root_->boundaries_.dimension));
            new_root->childern_[2] = std::move(root_);
            root_ = std::move(new_root);
            break;
          case 2:
            new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
                root_->boundaries_.x_min - root_->boundaries_.dimension, root_->boundaries_.x_max,
                root_->boundaries_.y_min - root_->boundaries_.dimension, root_->boundaries_.y_max));
            new_root->childern_[1] = std::move(root_);
            root_ = std::move(new_root);
            break;
          case 3:
            new_root = std::make_unique<PictureTreeNode>(Boundaries<int>(
                root_->boundaries_.x_min, root_->boundaries_.x_max + root_->boundaries_.dimension,
                root_->boundaries_.y_min - root_->boundaries_.dimension, root_->boundaries_.y_max));
            new_root->childern_[0] = std::move(root_);
            root_ = std::move(new_root);
            break;
          default:
            throw "Bad state (quarter)";
            break;
          }
        } while (!root_->boundaries_.Hit(c->boundaries_));
        // Now add the leaf
        root_->AddLeaf(c);
      }
    }
    to_steal_.clear();
  }

  // If you want to somehow see the tree
  friend std::ostream &operator<<(std::ostream &output, const PictureTree &p) {
    if (p.root_ != nullptr) {
      output << *p.root_;
      return output;
    }
    output << "Empty tree.";
    return output;
  }
  

private:
  /**
   * @brief Inner node of picture tree. PictureTree root is PictureTreeNode.
   *
   */
  class PictureTreeNode {
  public:
    PictureTreeNode(Boundaries<int> &&bound) : boundaries_(bound) {}
    ~PictureTreeNode(){};

    /**
     * @brief Add matrix leaf_size x leaf_size as leaft to this node
     *
     * @param ptn Matrix (leaf) to be added
     */
    void AddLeaf(PictureTreeNode *ptn) {
      // If were are right place than is done. mask it with whats already there
      if (boundaries_.dimension == leaf_size) {
        coordinates_ |= ptn->coordinates_;
      } else {
        // Were not there yet, recusively go into correct quater and go deeper
        // ASCI painting of quaters
        // y_max
        //   +----+-----+
        //   |  0 |  1  |
        //.  +----+-----+
        //   |  2 |  3  |
        //   +----+-----+
        // y_min        x_max
        // x_min
        auto quarter = GetQuarter(ptn->boundaries_);
        if (childern_[quarter] == nullptr) {
          switch (quarter) {
          case 0:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min, boundaries_.x_min + boundaries_.dimension / 2,
                                boundaries_.y_min + boundaries_.dimension / 2, boundaries_.y_max));
            break;
          case 1:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min + boundaries_.dimension / 2, boundaries_.x_max,
                                boundaries_.y_min + boundaries_.dimension / 2, boundaries_.y_max));
            break;
          case 2:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min, boundaries_.x_min + boundaries_.dimension / 2,
                                boundaries_.y_min, boundaries_.y_min + boundaries_.dimension / 2));
            break;
          case 3:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min + boundaries_.dimension / 2, boundaries_.x_max,
                                boundaries_.y_min, boundaries_.y_min + boundaries_.dimension / 2));
            break;
          default:
            throw "Bad state(quarter)";
            break;
          }
        }
        childern_[quarter]->AddLeaf(ptn);
      }
    }

    /**
     * @brief Get all leafs into vector. Recursive.
     *
     * @param ptn
     */
    void PrepareToSteal(std::vector<PictureTreeNode *> &ptn) {
      if (boundaries_.dimension == leaf_size) {
        ptn.push_back(this);
      } else {
        for (const auto &c : childern_) {
          if (c != nullptr) {
            c->PrepareToSteal(ptn);
          }
        }
      }
    }

    /**
     * @brief Checks if node contains point. Recursive
     *
     * @param x
     * @param y
     * @return true
     * @return false
     */
    bool Contains(const int &x, const int &y) {
      auto quarter = GetQuarter(x, y);
      // check for end of recursion
      if (boundaries_.dimension == leaf_size) {
        return coordinates_[static_cast<size_t>((y % leaf_size) * leaf_size + (x % leaf_size))];
      } else if (childern_[quarter] != nullptr)
        return childern_[quarter]->Contains(x, y);
      else
        return false;
    }

    /**
     * @brief Adding a new point to node. Recursive. Assuming pixel belongs to nodes boundaries. New
     * branches are created if needed.
     *
     * @param x
     * @param y
     */

    void AddPoint(const int &x, const int &y) {
      // chcek for end of recursion
      if (boundaries_.dimension == leaf_size) {
        coordinates_[static_cast<size_t>((y % leaf_size) * leaf_size + (x % leaf_size))] = true;
      } else {
        size_t quarter = GetQuarter(x, y);
        // ASCI painting of quaters
        // y_max
        //   +----+-----+
        //   |  0 |  1  |
        //.  +----+-----+
        //   |  2 |  3  |
        //   +----+-----+
        // y_min        x_max
        // x_min
        if (childern_[quarter] == nullptr) {
          switch (quarter) {
          case 0:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min, boundaries_.x_min + boundaries_.dimension / 2,
                                boundaries_.y_min + boundaries_.dimension / 2, boundaries_.y_max));
            break;
          case 1:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min + boundaries_.dimension / 2, boundaries_.x_max,
                                boundaries_.y_min + boundaries_.dimension / 2, boundaries_.y_max));
            break;
          case 2:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min, boundaries_.x_min + boundaries_.dimension / 2,
                                boundaries_.y_min, boundaries_.y_min + boundaries_.dimension / 2));
            break;
          case 3:
            childern_[quarter] = std::make_unique<PictureTreeNode>(
                Boundaries<int>(boundaries_.x_min + boundaries_.dimension / 2, boundaries_.x_max,
                                boundaries_.y_min, boundaries_.y_min + boundaries_.dimension / 2));
            break;
          default:
            throw;
          }
        }
        childern_[quarter]->AddPoint(x, y);
      }
    }

    /**
     * @brief If you want to display TreenNode...
     *
     * @param output
     * @param p Node to display
     * @return std::ostream&
     */
    friend std::ostream &operator<<(std::ostream &output, const PictureTreeNode &p) {
      if (p.boundaries_.dimension == leaf_size) {
        //output << p.boundaries_ << std::endl << "With points : " << std::endl;
        for (int y = 0; y < leaf_size; y++) {
          for (int x = 0; x < leaf_size; x++){}
            //output << p.coordinates_[(y % leaf_size) * leaf_size + (x % leaf_size)] << " ";
          
        }
        std::cout << (leaf_size*leaf_size) << "\n";
      }
      //output << std::endl << "Has childern : " << std::endl;
      for (auto &c : p.childern_)
        if (c != nullptr)
          output << *c << std::endl;
      //output << "---------------\n";
      return output;
    }

    std::unique_ptr<PictureTreeNode> childern_[4] = {nullptr, nullptr, nullptr, nullptr};
    // ASCI painting of quaters
    // y_max
    //   +----+-----+
    //   |  0 |  1  |
    //.  +----+-----+
    //   |  2 |  3  |
    //   +----+-----+
    // y_min        x_max
    // x_min

    // coordinates to store in leaf.
    std::bitset<static_cast<size_t>(leaf_size *leaf_size)> coordinates_;
    Boundaries<int> boundaries_; // Must be power of two
    size_t GetQuarter(Boundaries<int> &bond) { return GetQuarter(bond.x_min, bond.y_min); }
    size_t GetQuarter(const int &x, const int &y) {
      if (x < (boundaries_.x_min + boundaries_.dimension / 2))   // were in 0 or 2
        if (y < (boundaries_.y_min + boundaries_.dimension / 2)) // were in 2
          return 2;
        else // were in 0
          return 0;
      else {                                                     // were in 1 or 3
        if (y < (boundaries_.y_min + boundaries_.dimension / 2)) // were in 3
          return 3;
        else // were in 1
          return 1;
      }
    }
  };
  std::vector<PictureTreeNode *> to_steal_;
  std::unique_ptr<PictureTreeNode> root_;

};

#endif // CLUSTERER_PICTURE_TREE_H_
