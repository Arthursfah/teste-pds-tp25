#ifndef AMAZON_HPP
#define AMAZON_HPP

#include "base-sites.hpp"
#include <iostream>

class Amazon : public BaseSites {
public:
    Amazon() {}

    void setBaseUrl(const std::string& nome, const std::string& baseUrl) override {}
};
#endif // AMAZON_H 