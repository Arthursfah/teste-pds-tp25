#ifndef SCRAPER_H
#define SCRAPER_H

#include <string>
#include <vector>
#include <curl/curl.h>
#include <filesystem>
#include <gumbo.h>

#include "logger.h"
#include "config.h"

class WebScraper {

public:
    WebScraper(const Config& config, Logger& logger, const std::string& output_dir);
    ~WebScraper();
    bool scrape();
    bool scrape_um_site(const Config::SiteConfig& site, const std::string& searchTerm);

private:
    Config config;
    Logger& logger;
    CURL* curl;
    std::string output_directory_;

    struct ScrapedItem {
        std::string title;
        std::string price;
        std::string url;
    };

    void create_output_directory(const std::string& output);

    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp);
    std::string fetch_page(const std::string& url, int retries_left);
    std::vector<ScrapedItem> parse_mercado_livre(GumboNode* node);
    std::vector<ScrapedItem> parse_olx(GumboNode* node);
    std::vector<ScrapedItem> parse_amazon(GumboNode* node);

    void save_to_file(const std::vector<ScrapedItem>& items, const std::string& output);

    std::string trim(const std::string& str);
    void search_node(GumboNode* node, const std::string& tag, const std::string& attribute,
                     const std::string& value, std::vector<GumboNode*>& results);
};

#endif // SCRAPER_H