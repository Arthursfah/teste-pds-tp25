#include "scraper.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sys/stat.h>



WebScraper::WebScraper(const Config& cfg, Logger& log, const std::string& output_dir)
    : config(cfg), logger(log), output_directory_(output_dir) {
    curl = curl_easy_init();
    if (!curl) {
        logger.log(Logger::LogLevel::ERR, "Falha ao inicializar libcurl");
    }
}

WebScraper::~WebScraper() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

// Callback para armazenar o conteúdo da página baixada
size_t WebScraper::write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}

// Função para baixar uma página web com tentativas de retry
std::string WebScraper::fetch_page(const std::string& url, int retries_left) {
    std::string html_content;
    if (!curl) return "";

    // Configuração para simular um navegador
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0");
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt");
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookies.txt");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Referer: https://www.amazon.com.br/");
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: pt-BR,pt;q=0.8,en-US;q=0.5,en;q=0.3");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // --- A LINHA MÁGICA E IMPORTANTE ---
    // Pede para o cURL descomprimir automaticamente qualquer formato que ele suporte (gzip, br, etc.)
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    // Configurações padrão
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html_content);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        logger.log(Logger::LogLevel::ERR, "Falha ao baixar " + url + ": " + curl_easy_strerror(res));
        return "";
    }

    logger.log(Logger::LogLevel::INFO, "Pagina baixada com sucesso: " + url);
    return html_content;
}

// Função auxiliar para remover espaços em branco
std::string WebScraper::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \n\r\t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \n\r\t");
    return str.substr(first, last - first + 1);
}

// Função recursiva para buscar nós no HTML (ajustada para lidar com múltiplas classes)
void WebScraper::search_node(GumboNode* node, const std::string& tag, const std::string& attribute,
                             const std::string& value, std::vector<GumboNode*>& results) {
    if (node->type != GUMBO_NODE_ELEMENT) return;

    const char* tag_name = gumbo_normalized_tagname(node->v.element.tag);
    if (tag_name && tag == tag_name) {
        if (attribute.empty()) {
            // Sem filtro de atributo, adicionar direto
            results.push_back(node);
        } else {
            GumboAttribute* attr = gumbo_get_attribute(&node->v.element.attributes, attribute.c_str());
            if (attr) {
                std::string attr_value = attr->value;
                if (value.empty() || attr_value.find(value) != std::string::npos) {
                    results.push_back(node);
                }
            }
        }
    }

    for (unsigned int i = 0; i < node->v.element.children.length; ++i) {
        search_node(static_cast<GumboNode*>(node->v.element.children.data[i]), tag, attribute, value, results);
    }
}

// Parsing específico para Mercado Livre
std::vector<WebScraper::ScrapedItem> WebScraper::parse_mercado_livre(GumboNode* node) {
    std::vector<ScrapedItem> items;
    std::vector<GumboNode*> product_nodes;

    // Busca por elementos h3 com a classe poly-component__title-wrapper
    search_node(node, "h3", "class", "poly-component__title-wrapper", product_nodes);
    logger.log(Logger::LogLevel::INFO, "Número de itens encontrados no Mercado Livre: " + std::to_string(product_nodes.size()));

    for (auto* product : product_nodes) {
        ScrapedItem item;
        std::vector<GumboNode*> link_nodes, price_nodes;

        // Encontra o link dentro do h3
        search_node(product, "a", "class", "poly-component__title", link_nodes);
        if (!link_nodes.empty()) {
            std::string title;
            for (unsigned int i = 0; i < link_nodes[0]->v.element.children.length; ++i) {
                GumboNode* child = static_cast<GumboNode*>(link_nodes[0]->v.element.children.data[i]);
                if (child->type == GUMBO_NODE_TEXT) {
                    title += child->v.text.text;
                }
            }
            item.title = trim(title);
          //  logger.log(Logger::LogLevel::INFO, "Titulo encontrado: " + item.title); // usar so pra teste

            // Extrai o link do atributo href
            GumboAttribute* href = gumbo_get_attribute(&link_nodes[0]->v.element.attributes, "href");
            if (href) {
                item.url = href->value;
           //     logger.log(Logger::LogLevel::INFO, "Link encontrado: " + item.url); // usar so pra teste
            } else {
             //   logger.log(Logger::LogLevel::WARNING, "Link nao encontrado para um item no Mercado Livre"); // usar so pra teste
                item.url = "N/A";
            }
        } else {
          //  logger.log(Logger::LogLevel::WARNING, "Link nao encontrado para um item no Mercado Livre"); // usar so pra teste
            item.url = "N/A";
            item.title = "N/A";
        }

        // Encontra o preço (span com classe andes-money-amount__fraction)
        search_node(product->parent, "span", "class", "andes-money-amount__fraction", price_nodes);
        if (!price_nodes.empty()) {
            std::string price;
            for (unsigned int i = 0; i < price_nodes[0]->v.element.children.length; ++i) {
                GumboNode* child = static_cast<GumboNode*>(price_nodes[0]->v.element.children.data[i]);
                if (child->type == GUMBO_NODE_TEXT) {
                    price += child->v.text.text;
                }
            }
            item.price = trim(price);
         //   logger.log(Logger::LogLevel::INFO, "Preco encontrado: " + item.price); // usar so pra teste
        } else {
         //   logger.log(Logger::LogLevel::WARNING, "Preco nao encontrado para um item no Mercado Livre"); // usar so pra teste
            item.price = "N/A";
        }

        if (!item.title.empty() || !item.price.empty() || !item.url.empty()) {
            items.push_back(item);
           // logger.log(Logger::LogLevel::INFO, "Item encontrado no Mercado Livre: " + item.title + " | " + item.price + " | " + item.url); // usar so pra teste
        }
    }
    return items;
}

std::vector<WebScraper::ScrapedItem> WebScraper::parse_amazon(GumboNode* node) {
    logger.log(Logger::LogLevel::INFO, "INICIANDO PARSER AMAZON");
    std::vector<ScrapedItem> items;
    std::vector<GumboNode*> product_nodes;

    search_node(node, "div", "data-component-type", "s-search-result", product_nodes);
    logger.log(Logger::LogLevel::INFO, "Encontrados " + std::to_string(product_nodes.size()) + " cards.");

    for (auto* card : product_nodes) {
        ScrapedItem item;

        // --- TÍTULO ---
        std::vector<GumboNode*> title_nodes;
        search_node(card, "h2", "class", "a-size-base-plus", title_nodes);

        for (auto* a_node : title_nodes) {
            for (unsigned int i = 0; i < a_node->v.element.children.length; ++i) {
                GumboNode* span = static_cast<GumboNode*>(a_node->v.element.children.data[i]);
                if (span->type == GUMBO_NODE_ELEMENT && span->v.element.tag == GUMBO_TAG_SPAN) {
                    if (span->v.element.children.length > 0) {
                        GumboNode* text = static_cast<GumboNode*>(span->v.element.children.data[0]);
                        if (text->type == GUMBO_NODE_TEXT) {
                            std::string raw_title = trim(text->v.text.text);
                            if (!raw_title.empty() && raw_title.find("avaliação") == std::string::npos) {
                                item.title = raw_title;
                                break;
                            }
                        }
                    }
                }
            }
            std::vector<GumboNode*> a_nodes;
            search_node(card, "a", "class", "a-link-normal", a_nodes);
            for (auto* a : a_nodes) {
                GumboAttribute* href = gumbo_get_attribute(&a->v.element.attributes, "href");
                if (href && !item.title.empty()) {
                    item.url = "https://www.amazon.com.br" + std::string(href->value);
                    break;
                }
            }
        }

        // --- PREÇO ---
        std::vector<GumboNode*> price_nodes;
        search_node(card, "span", "class", "a-offscreen", price_nodes);
        for (auto* span : price_nodes) {
            if (span->v.element.children.length > 0) {
                GumboNode* text = static_cast<GumboNode*>(span->v.element.children.data[0]);
                if (text->type == GUMBO_NODE_TEXT) {
                    std::string preco_com_nbsp = trim(text->v.text.text);
                    std::string remover_nbsp = " ";

                    size_t pos = preco_com_nbsp.find(remover_nbsp);
                    if (pos != std::string::npos) {
                        preco_com_nbsp.replace(pos, remover_nbsp.length(), " ");
                    }

                    item.price = preco_com_nbsp;
                    break;
                }
            }
        }
        if (!item.title.empty()) {
            items.push_back(item);
        }
    }

    logger.log(Logger::LogLevel::INFO, std::to_string(items.size()) + " itens extraídos com sucesso.");
    return items;
}

// Parsing específico para OLX
std::vector<WebScraper::ScrapedItem> WebScraper::parse_olx(GumboNode* node) {
    logger.log(Logger::LogLevel::INFO, "--- INICIANDO PARSER DA OLX (VERSÃO DEFINITIVA) ---");
    std::vector<ScrapedItem> items;
    std::vector<GumboNode*> product_nodes;

    // 1. Encontra o "card" de cada anúncio na lista. O seletor mais estável é este:
    search_node(node, "li", "data-testid", "ad-list-item", product_nodes);
    logger.log(Logger::LogLevel::INFO, "Numero de itens encontrados na OLX: " + std::to_string(product_nodes.size()));

    if (product_nodes.empty()) {
        logger.log(Logger::LogLevel::ERR, "FALHA: Nenhum card de produto encontrado na OLX. O seletor 'li[data-testid=ad-list-item]' pode estar desatualizado.");
        return items;
    }

    for (auto* product_card : product_nodes) {
        ScrapedItem item;

        // 2. Encontra o link, que geralmente envolve todo o card
        std::vector<GumboNode*> link_nodes;
        search_node(product_card, "a", "class", "", link_nodes); // Procura por qualquer 'a' dentro do 'li'
        if (!link_nodes.empty()) {
            GumboAttribute* href = gumbo_get_attribute(&link_nodes[0]->v.element.attributes, "href");
            if (href) {
                item.url = href->value;
            }
        }

        // 3. Encontra o título, que está num H2 com uma classe específica
        std::vector<GumboNode*> title_nodes;
        search_node(product_card, "h2", "class", "olx-ad-card__title", title_nodes);
        if (!title_nodes.empty() && title_nodes[0]->v.element.children.length > 0) {
            GumboNode* title_text_node = static_cast<GumboNode*>(title_nodes[0]->v.element.children.data[0]);
            if (title_text_node->type == GUMBO_NODE_TEXT) {
                item.title = trim(title_text_node->v.text.text);
            }
        }

        // 4. Encontra o preço, que está num H3 com uma classe específica
        std::vector<GumboNode*> price_nodes;
        search_node(product_card, "h3", "class", "olx-ad-card__price", price_nodes);
        if (!price_nodes.empty() && price_nodes[0]->v.element.children.length > 0) {
            GumboNode* price_text_node = static_cast<GumboNode*>(price_nodes[0]->v.element.children.data[0]);
            if (price_text_node->type == GUMBO_NODE_TEXT) {
                item.price = trim(price_text_node->v.text.text);
            }
        }

        // Adiciona o item se ele tiver alguma informação útil
        if (!item.title.empty() && !item.price.empty()) {
            items.push_back(item);
        }
    }
    logger.log(Logger::LogLevel::INFO, "--- FIM DO PARSER DA OLX ---");
    return items;
}

// Em scraper.cpp
void WebScraper::save_to_file(const std::vector<ScrapedItem>& items, const std::string& file_path) {
    try {
        // Garante que o diretório de destino existe
        std::filesystem::path path_obj(file_path);
        std::filesystem::path dir = path_obj.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            logger.log(Logger::LogLevel::INFO, "Criando diretorio: " + dir.string());
            std::filesystem::create_directories(dir);
        }

        // Abre o arquivo para escrita
        std::ofstream file(file_path, std::ios::out);
        if (!file.is_open()) {
            logger.log(Logger::LogLevel::ERR, "Falha ao abrir arquivo para escrita: " + file_path);
            return;
        }

        if (items.empty()) {
            logger.log(Logger::LogLevel::WARNING, "Nenhum item encontrado para salvar em: " + file_path);
            file << "Nenhum item encontrado durante o scraping.\n"; // Escreve uma mensagem para não deixar o arquivo vazio
            file.close();
            return;
        }

        // Escreve os itens no arquivo
        for (const auto& item : items) {
            if (!item.title.empty() || !item.price.empty() || !item.url.empty()) {
                file << "Título: " << item.title << "\n"
                     << "Preço: " << item.price << "\n"
                     << "URL: " << item.url << "\n"
                     << "----------------------------------------\n";
            }
        }

        file.close();
        logger.log(Logger::LogLevel::INFO, "Dados salvos com sucesso em: " + file_path);

    } catch (const std::filesystem::filesystem_error& e) {
        logger.log(Logger::LogLevel::ERR, "Erro de filesystem ao salvar: " + std::string(e.what()));
    }
}


// Função principal de scraping
bool WebScraper::scrape() {
    if (!curl) return false;

    for (const auto& site : config.get_sites()) {
        logger.log(Logger::LogLevel::INFO, "Iniciando scraping em: " + site.name);
        std::string html = fetch_page(site.baseUrl, config.get_max_retries());
        if (html.empty()) continue;

        GumboOutput* output = gumbo_parse(html.c_str());
        std::vector<ScrapedItem> items;

        if (site.name == "Mercado Livre") {
            items = parse_mercado_livre(output->root);
            save_to_file(items, "../output/mercado_livre_data.txt");
        } else if (site.name == "OLX") {
            items = parse_olx(output->root);
            save_to_file(items, "../output/olx_data.txt");
        } else if (site.name == "Amazon") {
            items = parse_amazon(output->root);
            save_to_file(items, "../output/amazon_data.txt");
        }

        if (items.empty()) {
          // logger.log(Logger::LogLevel::WARNING, "Nenhum item encontrado em: " + site.name);  // usar so pra teste
        } else {
            save_to_file(items, site.output_file);
        }

        gumbo_destroy_output(&kGumboDefaultOptions, output);
    }
    return true;
}

bool WebScraper::scrape_um_site(const Config::SiteConfig& site, const std::string& searchTerm) {
    if (!curl) {
        logger.log(Logger::LogLevel::ERR, "CURL nao inicializado para raspagem de site unico.");
        return false;
    }

    logger.log(Logger::LogLevel::INFO, "Iniciando raspagem em: " + site.name + " para o termo: '" + searchTerm + "'");

    std::string searchUrl = site.baseUrl;  // Base URL do site
    if (site.name == "Mercado Livre") {
        std::string formattedSearchTerm = searchTerm;
        std::replace(formattedSearchTerm.begin(), formattedSearchTerm.end(), ' ', '-');
        searchUrl += formattedSearchTerm;
    } else if (site.name == "OLX") {
        searchUrl += "?q=" + searchTerm;
    } else if (site.name == "Amazon") {
        searchUrl += "?k=" + searchTerm;
    }

    std::string html = fetch_page(searchUrl, config.get_max_retries());
    if (html.empty()) {
        logger.log(Logger::LogLevel::ERR, "Falha ao obter HTML para " + site.name);
        return false;
    }

    GumboOutput* output = gumbo_parse(html.c_str());
    std::vector<ScrapedItem> items;

    if (site.name == "Mercado Livre") {
        items = parse_mercado_livre(output->root);
    } else if (site.name == "OLX") {
        items = parse_olx(output->root);
    } else if (site.name == "Amazon") {
        items = parse_amazon(output->root);
    } else {
        logger.log(Logger::LogLevel::WARNING, "Parser nao implementado para o site: " + site.name);
    }
    std::string debug_filename = site.name + "_debug_page.html";
    logger.log(Logger::LogLevel::INFO, "SALVANDO HTML PARA DEPURACAO EM: " + debug_filename);
    std::ofstream html_file(debug_filename);
    if(html_file.is_open()) {
        html_file << html;
        html_file.close();
    } else {
        logger.log(Logger::LogLevel::ERR, "Falha ao criar arquivo de debug HTML.");
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);

    std::string full_output_path = output_directory_ + "/" + site.output_file;
    save_to_file(items, full_output_path);

    logger.log(Logger::LogLevel::INFO, "Raspagem concluida para " + site.name);
    return true;
}



/* EM SALVAMENTO para baixar html e poder analisar o parser de cada site

if (site.name == "Amazon") {
    logger.log(Logger::LogLevel::INFO, "Salvando HTML recebido em amazon_page.html...");
    std::ofstream html_file("amazon_page.html");
    html_file << html;
    html_file.close();
} */