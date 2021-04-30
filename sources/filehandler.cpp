#include "filehandler.hpp"

#include <filesystem>

namespace fs = std::filesystem;


void FileHandler::handle(SSLClient *client, const GeminiRequest &request) {
    // TODO: get relative base to base of the 
    fs::path path(settings->getRoot());
    path /= request.getPath();
    
}
