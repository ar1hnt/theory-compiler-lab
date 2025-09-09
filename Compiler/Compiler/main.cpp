#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <tuple>
#include <queue>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

using namespace std;

// Структура, описывающая одно правило грамматики
struct Production {
    vector<string> lhs; // левая часть (список нетерминалов, может быть несколько)
    vector<string> rhs; // правая часть (терминалы и/или нетерминалы), пусто = ε (epsilon)
};

// --- Функция токенизации ---
// Разбивает строку (например "ABа") на последовательность токенов,
// используя множества терминалов и нетерминалов. Работает жадно (longest match).
vector<string> tokenize_with_sets(
    const string &s,
    const set<string> &terminals,
    const set<string> &nonterminals
) {
    vector<string> tokens;
    int i = 0;
    int L = (int)s.size();

    // находим максимальную длину токена (нужно для жадного поиска)
    int maxLen = 0;
    for (auto &x : terminals) maxLen = max(maxLen, (int)x.size());
    for (auto &x : nonterminals) maxLen = max(maxLen, (int)x.size());
    if (maxLen <= 0) maxLen = L; // fallback

    while (i < L) {
        string best;
        int upper = min(L - i, maxLen);
        // пробуем от длинного к короткому
        for (int len = upper; len >= 1; --len) {
            string sub = s.substr(i, len);
            if (terminals.count(sub) || nonterminals.count(sub)) {
                best = sub;
                break;
            }
        }
        if (best.empty()) {
            cerr << "Ошибка токенизации: не найден токен в строке \"" << s.substr(i)
                 << "\". Проверьте списки терминалов/нетерминалов.\n";
            exit(1);
        }
        tokens.push_back(best);
        i += (int)best.size();
    }
    return tokens;
}

// --- Проверка одного слова на выводимость ---
bool checkWord(
    const string &word,
    const set<string> &terminals,
    const set<string> &nonterminals,
    const vector<Production> &productions,
    const string &start
) {
    // Стартовая форма: просто [S]
    vector<string> startForm = { start };

    // Функция кодирования формы в строку (для visited-множества)
    auto encodeForm = [&](const vector<string> &frm) {
        string key;
        for (auto &tk : frm) {
            key += "\x1F"; // спец-разделитель
            key += tk;
        }
        return key;
    };

    // Проверка: все ли токены — терминалы
    auto isAllTerminals = [&](const vector<string> &frm) {
        for (auto &tk : frm)
            if (!terminals.count(tk)) return false;
        return true;
    };

    // Суммарная длина терминалов (в байтах)
    auto totalTerminalBytes = [&](const vector<string> &frm) {
        size_t s = 0;
        for (auto &tk : frm)
            if (terminals.count(tk)) s += tk.size();
        return s;
    };

    // Конкатенация терминальных токенов в строку
    auto concatTerminals = [&](const vector<string> &frm) {
        string s;
        for (auto &tk : frm) s += tk;
        return s;
    };

    queue<vector<string>> q;
    unordered_set<string> visited;

    q.push(startForm);
    visited.insert(encodeForm(startForm));

    const size_t MAX_VISITED = 2000000; // ограничение для защиты от взрыва состояний

    while (!q.empty()) {
        auto cur = q.front();
        q.pop();

        // Если длина терминалов превысила длину слова — дальше нет смысла
        if (totalTerminalBytes(cur) > word.size()) continue;

        // Если все терминалы → проверим совпадение
        if (isAllTerminals(cur)) {
            if (concatTerminals(cur) == word) return true;
            continue;
        }

        // Применяем все правила ко всем позициям
        for (auto &prod : productions) {
            int L = (int)cur.size();
            int M = (int)prod.lhs.size();
            for (int pos = 0; pos + M <= L; ++pos) {
                bool match = true;
                for (int j = 0; j < M; ++j) {
                    if (cur[pos + j] != prod.lhs[j]) { match = false; break; }
                }
                if (!match) continue;

                // Строим новую форму
                vector<string> next;
                next.reserve(L - M + prod.rhs.size());
                for (int i = 0; i < pos; ++i) next.push_back(cur[i]);
                for (auto &tk : prod.rhs) next.push_back(tk);
                for (int i = pos + M; i < L; ++i) next.push_back(cur[i]);

                if (totalTerminalBytes(next) > word.size()) continue;

                string key = encodeForm(next);
                if (!visited.count(key)) {
                    visited.insert(key);
                    q.push(next);
                }

                if (visited.size() > MAX_VISITED) {
                    cerr << "Достигнут лимит состояний, остановка.\n";
                    return false;
                }
            }
        }
    }

    return false;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Открываем input.txt
    ifstream fin("./../../../../../Compiler/input.txt");
    if (!fin) {
        cerr << "Не удалось открыть input.txt. Текущая директория: "
             << filesystem::current_path() << "\n";
        return 1;
    }

    set<string> terminals;
    set<string> nonterminals;
    vector<Production> productions;

    // Чтение терминалов
    int t; fin >> t;
    for (int i = 0; i < t; ++i) {
        string tok; fin >> tok;
        terminals.insert(tok);
    }

    // Чтение нетерминалов
    int n; fin >> n;
    for (int i = 0; i < n; ++i) {
        string tok; fin >> tok;
        nonterminals.insert(tok);
    }

    // Стартовый символ
    string start; fin >> start;
    nonterminals.insert(start);

    // Чтение правил
    int r; fin >> r;
    vector<pair<string,string>> rawRules;
    rawRules.reserve(r);

    for (int i = 0; i < r; ++i) {
        string rule; fin >> rule;
        auto pos = rule.find("->");
        if (pos == string::npos) {
            cerr << "Ошибка формата правила: " << rule << "\n";
            return 1;
        }
        string lhs_raw = rule.substr(0, pos);
        string rhs_raw = rule.substr(pos + 2);
        rawRules.emplace_back(lhs_raw, rhs_raw);
    }

    // Обрабатываем правила
    for (auto &pr : rawRules) {
        const string &lhs_raw = pr.first;
        const string &rhs_raw = pr.second;

        // epsilon-правило
        if (rhs_raw == "e") {
            vector<string> lhs_tokens = tokenize_with_sets(lhs_raw, terminals, nonterminals);
            productions.push_back({lhs_tokens, {}});
            continue;
        }

        // токенизация
        vector<string> lhs_tokens = tokenize_with_sets(lhs_raw, terminals, nonterminals);
        vector<string> rhs_tokens = tokenize_with_sets(rhs_raw, terminals, nonterminals);

        // если RHS случайно оказался "e"
        if (rhs_tokens.size() == 1 && rhs_tokens[0] == "e") rhs_tokens.clear();

        productions.push_back({lhs_tokens, rhs_tokens});
    }

    // Чтение количества слов
    int m; fin >> m;
    vector<string> words(m);
    for (int i = 0; i < m; ++i) fin >> words[i];

    // Проверка каждого слова
    for (auto &w : words) {
        bool ok = checkWord(w, terminals, nonterminals, productions, start);
        cout << "Слово \"" << w << "\": "
             << (ok ? "можно" : "нельзя")
             << " вывести из грамматики.\n";
    }

    return 0;
}
