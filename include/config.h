#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <algorithm>

class Config {
public:
    Config();
    

    std::vector<SiteConfig> get_sites() const;
    int get_max_retries() const;
    bool get_verbose() const;
    void add_site(const std::string& name, const std::string& baseUrl);
    void set_max_retries(int retries);
    void set_verbose(bool verbose);

private:
    std::vector<SiteConfig> sites;
    int max_retries;
    bool verbose;
    struct SiteConfig {           
        std::string name;
        std::string baseUrl;
        std::string output_file;
    };
};

#endif // CONFIG_H