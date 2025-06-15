#ifndef OLX_HPP
#define OLX_HPP

#include "base-sites.hpp"
#include <iostream>

class Olx : public BaseSites {
public:
    Olx() {}

    void setBaseUrl(const std::string& nome, const std::string& baseUrl) override {}
};
#endif // OLX_H