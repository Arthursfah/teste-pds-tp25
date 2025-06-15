#include "base-sites.hpp"
#include "mlivre.hpp"
#include "olx.hpp"
#include "amazon.hpp"
#include <iostream>

    // BaseSites class implementation
    std::string& BaseSites::getTitulo() const { return produto.titulo; }
    std::string& BaseSites::getUrl() const { return produto.url; }
    int BaseSites::getPreco() const { return produto.preco; }

    void BaseSites::addProduto(const Produto& novoProduto) {
        produtos[novoProduto.preco] = novoProduto;
    }

    // Mercado Livre class implementation
    MercadoLivre::MercadoLivre() {
        MercadoLivre::nomeSite = "Mercado Livre";
        MercadoLivre::baseUrlSite = "https://lista.mercadolivre.com.br/";
    }
    void setBaseUrl(std::string searchTerm) override {
        std::string formattedSearchTerm = searchTerm;
        std::replace(formattedSearchTerm.begin(), formattedSearchTerm.end(), ' ', '-');
        MercadoLivre::baseUrlSite += formattedSearchTerm;
    }
   
    // Olx class implementation
    Olx::Olx() {
        Olx::nomeSite = "OLX";
        Olx::baseUrlSite = "https://www.olx.com.br/brasil";
    }
    void setBaseUrl(std::string searchTerm) override {
        Olx::baseUrlSite += "?q=" + searchTerm;
    }

    // Amazon class implementation
    Amazon::Amazon() {
        Amazon::nomeSite = "Amazon";
        Amazon::baseUrlSite = "https://www.amazon.com.br/s";
    }
    void setBaseUrl(std::string searchTerm) override {
        Amazon::baseUrlSite  += "?k=" + searchTerm;
    }





























std::ostream& operator<<(std::ostream& os, const Site& site) {
    os << "Site: " << site.getName() << ", Base URL: " << site.getBaseUrl()
       << ", Output File: " << site.getOutputFile();
    return os;
}
std::vector<Site> getDefaultSites() {
    return {
        Site("Mercado Livre", "https://lista.mercadolivre.com.br/"),
        Site("OLX", "https://www.olx.com.br/brasil"),
        Site("Amazon", "https://www.amazon.com.br/s")
    };
}
std::vector<Site> getCustomSites(const std::vector<std::pair<std::string, std::string>>& customSites) {
    std::vector<Site> sites;
    for (const auto& site : customSites) {
        sites.emplace_back(site.first, site.second);
    }
    return sites;
}
