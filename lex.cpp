// Example extremely naive lexer/parser.
// To test:
//   g++ -Ofast lex.cpp -o lex && time ./lex files/source_1M.txt

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>
#include <vector>
#include <memory>

// ============================================================
//                      Phase 1: Lexing
// ============================================================

enum TokenKind {
    TOKEN_FUN,
    TOKEN_LET,
    TOKEN_COLON_EQ,
    TOKEN_ARROW,
    TOKEN_L_PAREN,
    TOKEN_R_PAREN,
    TOKEN_SEMICOLON,
    TOKEN_STRING_LIT,
    TOKEN_IDENTIFIER,
};

const char* token_name(TokenKind kind) {
    switch (kind) {
        case TOKEN_FUN:        return "fun";
        case TOKEN_LET:        return "let";
        case TOKEN_COLON_EQ:   return ":=";
        case TOKEN_ARROW:      return "=>";
        case TOKEN_L_PAREN:    return "(";
        case TOKEN_R_PAREN:    return ")";
        case TOKEN_SEMICOLON:  return ";";
        case TOKEN_STRING_LIT: return "string-literal";
        case TOKEN_IDENTIFIER: return "identifier";
    }
    return "???";
}

struct Token {
    TokenKind kind;
    // For TOKEN_STRING_LIT and TOKEN_IDENTIFIER these two fields point
    // at the string literal or the identifier's name respectively.
    size_t string_length;
    const char* string_data;

    std::string get_string() const {
        return std::string(string_data, string_length);
    }

    std::string debug_name() const {
        switch (kind) {
            case TOKEN_FUN:
            case TOKEN_LET:
                return std::string("\x1b[95m") + token_name(kind) + "\x1b[0m";
            case TOKEN_STRING_LIT:
                return "\x1b[92m\"" + get_string() + "\x1b[0m";
            case TOKEN_IDENTIFIER:
                return "\x1b[94m" + get_string() + "\x1b[0m";
            default:
                return token_name(kind);
        }
    }
};

constexpr uint16_t two_chars(const char* s) {
    return static_cast<uint16_t>(s[0]) + (static_cast<uint16_t>(s[1]) << 8);
}

// Global arrays are default initialized, so these are false.
bool identifier_start_table[256];
bool identifier_valid_table[256];

void setup_lookup_tables() {
    for (char c : "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_") {
        if (c == 0)
            continue;
        identifier_start_table[c] = true;
        identifier_valid_table[c] = true;
    }

    for (char c : "0123456789") {
        if (c == 0)
            continue;
        identifier_valid_table[c] = true;
    }
}

std::vector<Token> lex(size_t length, const char* source) {
    std::vector<Token> tokens;

    const char* source_start = source;
    const char* end = source + length;
    while (source < end) {
        // Handle length two symbols, if there're at least two bytes left.
        if (source < end - 1) {
            switch (*(const uint16_t*) source) {
                case two_chars("//"):
                    source += 2;
                    while (*source != '\n')
                        source++;
                    continue;
                case two_chars("/*"): {
                    source += 2;
                    int depth = 1;
                    while (depth) {
                        if (source >= end - 1) {
                            std::cerr << "Lex error: Unterminated block comment\n";
                            exit(3);
                        }
                        uint16_t scan_pair = *(const uint16_t*) source;
                        if (scan_pair == two_chars("/*")) {
                            source += 2;
                            depth++;
                        } else if (scan_pair == two_chars("*/")) {
                            source += 2;
                            depth--;
                        } else {
                            source++;
                        }
                    }
                    continue;
                }
                case two_chars(":="):
                    source += 2;
                    tokens.push_back({TOKEN_COLON_EQ, 0, nullptr});
                    continue;
                case two_chars("=>"):
                    source += 2;
                    tokens.push_back({TOKEN_ARROW, 0, nullptr});
                    continue;
            }
        }

        // Consume a single character.
        char c = *source++;

        // Handle length one symbols and also string literals.
        switch (c) {
            case '(':
                tokens.push_back({TOKEN_L_PAREN, 0, nullptr});
                continue;
            case ')':
                tokens.push_back({TOKEN_R_PAREN, 0, nullptr});
                continue;
            case ';':
                tokens.push_back({TOKEN_SEMICOLON, 0, nullptr});
                continue;
            case '\n':
            case ' ':
            case '\t':
                continue;
            case '"': {
                const char* begin = source;
                while (true) {
                    if (source >= end) {
                        std::cerr << "Lex error: Unterminated string literal\n";
                        exit(3);
                    }
                    char scan_c = *source++;
                    if (scan_c == '"')
                        break;
                    if (scan_c == '\\')
                        source++;
                }
                tokens.push_back({TOKEN_STRING_LIT, (size_t) (source - 1 - begin), begin});
                continue;
            }
        }

        if (not identifier_start_table[c]) {
            std::cerr << "Lex error: Unexpected character: " << c
                << " at " << (source - source_start) << " bytes\n";
            exit(3);
        }

        // Parse an identifier or keyword.
        const char* begin = source - 1;
        while (identifier_valid_table[*source])
            source++;

        size_t string_length = source - begin;

        if (string_length == 3 and begin[0] == 'l' and begin[1] == 'e' and begin[2] == 't') {
            tokens.push_back({TOKEN_LET, 0, nullptr});
        } else if (string_length == 3 and begin[0] == 'f' and begin[1] == 'u' and begin[2] == 'n') {
            tokens.push_back({TOKEN_FUN, 0, nullptr});
        } else {
            tokens.push_back({TOKEN_IDENTIFIER, string_length, begin});
        }
    }
    return tokens;
}


// ============================================================
//                      Phase 2: Parsing
// ============================================================

// Of course, having all these heap-allocated AST nodes with
// virtual methods is slow, this is just a simple example.
struct Expr {
    virtual ~Expr() {}
};

struct ExprVariable : public Expr {
    std::string name;

    ExprVariable(std::string name) : name(name) {}
};

struct ExprStringLiteral : public Expr {
    std::string literal;

    ExprStringLiteral(std::string literal) : literal(literal) {}
};

struct ExprLambda : public Expr {
    std::string argument_name;
    std::unique_ptr<Expr> body;

    ExprLambda(std::string argument_name, std::unique_ptr<Expr> body)
        : argument_name(argument_name), body(std::move(body)) {}
};

struct ExprApplication : public Expr {
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;

    ExprApplication(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
        : lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

struct Declaration {
    std::string name;
    std::unique_ptr<Expr> value;
};

#define CHECK_NOT_OFF_END \
    do { \
        if (cursor >= tokens.size()) { \
            std::cerr << "Parse error: Unexpected end of file\n"; \
            exit(3); \
        } \
    } while(0)

#define EXPECT(expected_kind) \
    do { \
        CHECK_NOT_OFF_END; \
        if (tokens[cursor++].kind != (expected_kind)) { \
            std::cerr << "Parse error: Unexpected token " \
                << tokens[cursor - 1].debug_name() << " at " << cursor \
                << ", wanted " << token_name(expected_kind) << "\n"; \
            exit(3); \
        } \
    } while(0)

std::unique_ptr<Expr> parse_expr(size_t& cursor, const std::vector<Token>& tokens);

size_t application_count = 0;
size_t lambda_count = 0;
size_t string_literal_count = 0;

std::unique_ptr<Expr> parse_expr_atom(size_t& cursor, const std::vector<Token>& tokens) {
    CHECK_NOT_OFF_END;
    switch (tokens[cursor++].kind) {
        case TOKEN_IDENTIFIER:
            return std::make_unique<ExprVariable>(tokens[cursor - 1].get_string());
        case TOKEN_STRING_LIT:
            string_literal_count++;
            return std::make_unique<ExprStringLiteral>(tokens[cursor - 1].get_string());
        case TOKEN_FUN: {
            lambda_count++;
            EXPECT(TOKEN_IDENTIFIER);
            std::string argument_name = tokens[cursor - 1].get_string();
            EXPECT(TOKEN_ARROW);
            std::unique_ptr<Expr> body = parse_expr(cursor, tokens);
            return std::make_unique<ExprLambda>(argument_name, std::move(body));
        }
        case TOKEN_L_PAREN: {
            auto value = parse_expr(cursor, tokens);
            EXPECT(TOKEN_R_PAREN);
            return value;
        }
        default:
            std::cerr << "Parse error: Unexpected token "
                << tokens[cursor - 1].debug_name() <<  " in expression\n";
            exit(3);
    }
}

std::unique_ptr<Expr> parse_expr(size_t& cursor, const std::vector<Token>& tokens) {
    auto expr = parse_expr_atom(cursor, tokens);
    while (cursor < tokens.size()) {
        switch (tokens[cursor].kind) {
            case TOKEN_IDENTIFIER:
            case TOKEN_STRING_LIT:
            case TOKEN_FUN:
            case TOKEN_L_PAREN:
                application_count++;
                expr = std::make_unique<ExprApplication>(
                    std::move(expr), std::move(parse_expr(cursor, tokens))
                );
                break;
            default:
                return expr;
        }
    }
    // Suppress the compiler warning by returning something in this unreachable code.
    return expr;
}

std::vector<Declaration> parse(const std::vector<Token>& tokens) {
    std::vector<Declaration> declarations;
    size_t cursor = 0;

    while (cursor < tokens.size()) {
        EXPECT(TOKEN_LET);
        EXPECT(TOKEN_IDENTIFIER);
        std::string declaration_name = tokens[cursor - 1].get_string();
        EXPECT(TOKEN_COLON_EQ);
        std::unique_ptr<Expr> value = parse_expr(cursor, tokens);
        EXPECT(TOKEN_SEMICOLON);
        declarations.push_back({declaration_name, std::move(value)});
    }

    return declarations;
}

// ============================================================
//                          Testing
// ============================================================

struct Timer {
    std::string message;
    std::chrono::time_point<std::chrono::high_resolution_clock> timer_start;

    Timer(std::string message) : message(message) {
        timer_start = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        auto timer_stop = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = timer_stop - timer_start;
        std::cout << message << " took: " << elapsed.count() << "s\n";
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: lex input_file\n";
        return 1;
    }

    setup_lookup_tables();

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open: " << argv[1] << "\n";
        return 2;
    }
    struct stat st;
    fstat(fd, &st);
    size_t file_length = st.st_size;
    std::cout << "Mapping " << file_length << " bytes\n\n";
    char* file_mapping = (char*) mmap(nullptr, file_length, PROT_READ, MAP_PRIVATE, fd, 0);

    Timer t1("Lexing");
    std::vector<Token> tokens = lex(file_length, file_mapping);
    t1.stop();

#ifdef DEBUG_PRINT_TOKENS
    std::cout << "Tokens:";
    for (auto token : tokens)
        std::cout << " " << token.debug_name();
    std::cout << "\n";
#endif

    Timer t2("Parsing");
    std::vector<Declaration> declarations = parse(tokens);
    t2.stop();

    std::cout << "\n";
    std::cout << "Token count:          " << tokens.size() << "\n";
    std::cout << "Declaration count:    " << declarations.size() << "\n";
    std::cout << "Lambda count:         " << lambda_count << "\n";
    std::cout << "Application count:    " << application_count << "\n";
    std::cout << "String literal count: " << string_literal_count << "\n";
}
