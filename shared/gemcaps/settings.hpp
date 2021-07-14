#ifndef __GEMCAPS_SHARED_SETTINGS__
#define __GEMCAPS_SHARED_SETTINGS__

#include <string>
#include <sstream>
#include <exception>

#include <yaml-cpp/yaml.h>

/**
 * An exception that is thrown when Invalid settings have been passed
 */
class InvalidSettingsException : public std::exception {
private:
    std::string message;
    YAML::Mark mark;
public:
    /**
     * Create an InvalidSettingsException
     * 
     * @param mark the position in the file of the exception
     * @param message the message of why an exception occurred
     */
    InvalidSettingsException(YAML::Mark mark, std::string message) noexcept
        : mark(mark),
          message(message) {}
    
    /**
     * Get the error message
     * 
     * @return the error message
     */
    const char *what() const noexcept { return message.c_str(); }

    /**
     * Get the node that caused the exception
     * 
     * @return the exception mark
     */
    YAML::Mark getMark() const noexcept { return mark; }

    /**
     * Get the error message along with the filename
     * 
     * @param file filename
     * 
     * @return the error message
     */
    std::string getMessage(std::string file) const noexcept {
        std::ostringstream oss;
        oss << file << ":" << mark.line << ":" << mark.column << ": " << message;
        return oss.str();
    }
};

/**
 * Get a property from a node
 * 
 * @param node
 * @param property
 * 
 * @return found value
 * 
 * @throws InvalidSettingsException if the value could not be retreived
 */
template<class T>
T getProperty(YAML::Node node, std::string property) {
    if (!node[property].IsDefined()) {
        throw InvalidSettingsException(node.Mark(), property + " is a required property");
    }
    try {
        return node[property].as<T>();
    } catch (YAML::RepresentationException e) {
        throw InvalidSettingsException(e.mark, e.what());
    }
}

/**
 * Get a property from a node
 * 
 * @param node node to get the property from
 * @param property property to get
 * @param missing the value to return if the value is not present
 * 
 * @return found value
 * 
 * @throws InvalidSettingsException if the value could not be retreived
 */
template<class T>
T getProperty(YAML::Node node, std::string property, T missing) {
    try {
        return node[property].as<T>(missing);
    } catch (YAML::RepresentationException e) {
        throw InvalidSettingsException(e.mark, e.what());
    }
}


#endif