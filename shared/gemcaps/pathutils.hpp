#ifndef __GEMCAPS_SHARED_PATHUTILS__
#define __GEMCAPS_SHARED_PATHUTILS__

#include <string>
#include <vector>


namespace path {
/**
 * Join two paths together
 * 
 * @param root the root path
 * @param path the path to add
 * 
 * @return the joined paths
 */
std::string join(std::string root, std::string path) noexcept;
/**
 * Join a vector of paths together
 * 
 * @param paths paths to join
 * @param join join token
 * 
 * @return the joined path
 */
std::string join(std::vector<std::string> paths, std::string join = "/") noexcept;
/**
 * Get the last element in the path
 * 
 * @param path path
 * 
 * @return the basename of the given path
 */
std::string basename(std::string path) noexcept;
/**
 * Remove the parent element in the path
 * 
 * @param path path
 * 
 * @return the dirname of the given path
 */
std::string dirname(std::string path) noexcept;
/**
 * Split the path into individual elements
 * 
 * @param path path
 * 
 * @return vector of each element in the path
 */
std::vector<std::string> split(std::string path) noexcept;
/**
 * Check if a path is a subpath of another
 * 
 * @param path the base path
 * @param subpath the subpath
 * 
 * @return whether subpath is a child of path
 */
bool isSubpath(std::string path, std::string subpath) noexcept;
/**
 * Make a path relative to another
 * 
 * @param path path to make relative
 * @param rel base path
 * 
 * @return path relative to rel
 */
std::string relpath(std::string path, std::string rel) noexcept;
/**
 * Remove up directories and redundant paths such as (./ ../ //)
 * 
 * @param path path
 * 
 * @return an equivalent path without unecessary up dirs
 * 
 * @note This will not take symlinks into account
 */
std::string delUps(std::string path) noexcept;
}

#endif