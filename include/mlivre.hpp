#ifndef  MLIVRE_HPP
#define  MLIVRE_HPP

#include "base-sites.hpp"
#include <iostream>

class MercadoLivre : public BaseSites {
public:
    MercadoLivre() {}

    void setBaseUrl(const std::string& nome, const std::string& baseUrl) override {}
    
};
#endif // MLIVRE_H
