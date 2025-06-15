#ifndef BASE_SITES_HPP
#define BASE_SITES_HPP

#include <string>
#include <map>

class BaseSites {
public:
    BaseSites() = default;
    ~Site() = default;

    std::string& getTitulo() const { return produto.titulo; }
    std::string& getUrl() const { return produto.url; }
    int getPreco() const { return produto.preco; } 

    virtual void setBaseUrl(const std::string& nome, const std::string& baseUrl) = 0;

    // Método para adicionar produtos ao mapa
    void addProduto(const Produto& novoProduto) {}

private:
    static std::string nomeSite;        // Atributos do site
    static std::string baseUrlSite;     // Atributos do site

    struct Produto {
        std::string descricao;
        int preco = 0;
        std::string url;
    };

    Produto produto;  
    std::map<int, Produto> produtos;
};

#endif 