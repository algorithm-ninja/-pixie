#include <arpa/inet.h>
#include <config_file.h>
#include <json/json.h>
#include <algorithm>
#include <climits>
#include <fstream>
#include <utility>

DownloadConfig::DownloadConfig(
    const std::string& subnet,
    std::vector<std::pair<std::string, std::string>> files,
    chunk_size_t chunk_size)
    : chunk_size(chunk_size) {
    auto slash = subnet.begin() + subnet.find("/");
    if (slash == subnet.end())
        throw std::runtime_error("Invalid subnet given!");
    std::string ip{subnet.begin(), slash};
    std::string mask{slash + 1, subnet.end()};
    ip_address = inet_addr(ip.c_str());
    if (ip_address == INADDR_NONE)
        throw std::runtime_error("Invalid subnet given!");
    int integer_mask = std::stoi(mask);
    if (integer_mask < 0 || integer_mask > 32)
        throw std::runtime_error("Invalid subnet given!");
    subnet_mask = htonl(UINT_MAX << (32 - integer_mask));

    // Sort files to avoid order changes causing hash changes.
    std::sort(files.begin(), files.end());
    SHA224 hasher;
    for (auto file : files)
        file_data.emplace(file.first, File{file.second, chunk_size, hasher});
    config_hash = hasher.get();
}

std::vector<DownloadConfig> parse_config(
    const std::vector<std::string>& configs) {
    using namespace std::string_literals;
    std::vector<DownloadConfig> configurations;
    for (const auto& config : configs) {
        std::ifstream config_file(config,
                                  std::ifstream::binary | std::ifstream::in);
        Json::Value config_root;
        config_file >> config_root;
        std::string subnet = config_root.get("subnet", "").asString();
        if (subnet == "")
            throw std::runtime_error("Subnet missing in the config file!");
        chunk_size_t chunk_size =
            config_root.get("chunk_size", DEFAULT_CHUNK_SIZE).asUInt();
        auto& file_list = config_root["files"];
        if (!file_list.isObject()) throw std::runtime_error("Wrong file list!");
        std::vector<std::pair<std::string, std::string>> files;
        std::string canonical_path;
        if (config.front() != '/')
            canonical_path = config;
        else
            canonical_path = "./"s + config;
        while (canonical_path.back() != '/') canonical_path.pop_back();
        for (auto fname : file_list.getMemberNames()) {
            std::string path = file_list.get(fname, "").asString();
            if (path.front() != '/') path = canonical_path + path;
            files.push_back(make_pair(fname, path));
        }
        configurations.emplace_back(subnet, files, chunk_size);
    }
    return configurations;
}
