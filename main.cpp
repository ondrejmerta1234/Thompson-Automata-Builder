#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "sample.h"

struct Transition{
    size_t to;
    int symbol; // -1 for epsilon
};

//NFA with multiple accept states
struct NFA{
    size_t start;
    std::unordered_set<size_t> finishes;
    std::unordered_map<size_t, std::vector<Transition>> transitions;
};


//Build NFA fragments for each regex component using Thompson's construction

NFA buildEpsilon(size_t currStateNum){
    NFA fragment;
    fragment.start = currStateNum + 1;
    fragment.finishes.insert(currStateNum + 2);
    fragment.transitions[fragment.start].push_back({*(fragment.finishes).begin(), -1});
    return fragment;
}

NFA buildSymbol(size_t currStateNum, int symbol){
    NFA fragment;
    fragment.start = currStateNum + 1;
    fragment.finishes.insert(currStateNum + 2);
    fragment.transitions[fragment.start].push_back({*(fragment.finishes).begin(), symbol});
    return fragment;
}

NFA buildEmpty(size_t currStateNum){
    NFA fragment;
    fragment.start = currStateNum + 1;
    fragment.finishes.insert(currStateNum + 2);
    return fragment;
}

NFA buildAlternation(size_t currStateNum, const NFA & left, const NFA & right)
{
    NFA fragment;
    fragment.start = currStateNum + 1;
    fragment.finishes.insert(currStateNum + 2);

    fragment.transitions = left.transitions;
    for (const auto& [key, vector] : right.transitions) {
        fragment.transitions[key].insert(fragment.transitions[key].end(), vector.begin(), vector.end());
    }

    fragment.transitions[fragment.start].push_back({left.start, -1});
    fragment.transitions[fragment.start].push_back({right.start, -1});
    fragment.transitions[*(left.finishes).begin()].push_back({*(fragment.finishes).begin(), -1});
    fragment.transitions[*(right.finishes).begin()].push_back({*(fragment.finishes).begin(), -1});

    return fragment;
}

NFA buildConcatenation(const NFA & left, const NFA & right)
{
    NFA fragment;
    fragment.start = left.start;
    fragment.finishes.insert(*(right.finishes).begin());

    fragment.transitions = left.transitions;
    for (const auto& [key, vector] : right.transitions) {
        fragment.transitions[key].insert(fragment.transitions[key].end(), vector.begin(), vector.end());
    }

    fragment.transitions[*(left.finishes).begin()].push_back({right.start, -1});
    return fragment;
}

NFA buildIteration(size_t currStateNum, const NFA & node)
{
    NFA fragment;
    fragment.start = currStateNum + 1;
    fragment.finishes.insert(currStateNum + 2);

    for (const auto& [key, vector] : node.transitions) {
        fragment.transitions[key].insert(fragment.transitions[key].end(), vector.begin(), vector.end());
    }

    fragment.transitions[fragment.start].push_back({node.start, -1});
    fragment.transitions[*(node.finishes).begin()].push_back({*(fragment.finishes).begin(), -1});
    fragment.transitions[fragment.start].push_back({*(fragment.finishes).begin(), -1});
    fragment.transitions[*(node.finishes).begin()].push_back({node.start, -1});

    return fragment;
}


//Recursively build NFA from regex
NFA buildNFA(const regexp::RegExp & regexp, size_t currStateNum)
{
    // Base cases
   if(auto s = std::get_if<std::unique_ptr<regexp::Symbol>>(&regexp)) {
       return buildSymbol(currStateNum, (*s)->m_symbol);
   }
   // Epsilon case
   if(std::get_if<std::unique_ptr<regexp::Epsilon>>(&regexp)) {
       return buildEpsilon(currStateNum);
   }
   // Empty case
   if(std::get_if<std::unique_ptr<regexp::Empty>>(&regexp)) {
       return buildEmpty(currStateNum);
   }

   // Recursive cases
   if(auto s = std::get_if<std::unique_ptr<regexp::Alternation>>(&regexp)) {
        //Alternation +
         NFA left = buildNFA((*s)->m_left, currStateNum);
         NFA right = buildNFA((*s)->m_right, *(left.finishes).begin());
         return buildAlternation(*(right.finishes).begin(), left, right);
   }
   if(auto s = std::get_if<std::unique_ptr<regexp::Concatenation>>(&regexp)) {
        //Concatenation .
        NFA left = buildNFA((*s)->m_left, currStateNum);
        NFA right = buildNFA((*s)->m_right, *(left.finishes).begin());
        return buildConcatenation(left, right);
   }
    if(auto s = std::get_if<std::unique_ptr<regexp::Iteration>>(&regexp)) {
        //Iteration *
        NFA node = buildNFA((*s)->m_node, currStateNum);
        return buildIteration(*(node.finishes).begin(), node);
    }
    throw std::runtime_error("Unknown");
}


//Epsilon-closure computation using BFS
std::set<size_t> epsClosure(const NFA & nfa, size_t start)
{
    std::set<size_t> result;
    std::queue<size_t> q;
    q.push(start);
    result.insert(start);
    while(!q.empty())
    {
        auto state = q.front();
        q.pop();
        auto it = nfa.transitions.find(state);
        if (it == nfa.transitions.end()) continue;

        for(const auto & tr : it->second){
            if(tr.symbol == -1 && !result.contains(tr.to)){
                result.insert(tr.to);
                q.push(tr.to);
            }
        }
    }
    return result;
}

//Epsilon removal from NFA using epsilon-closures
NFA epsilonRemover(const NFA & nfa)
{
    NFA result;
    std::unordered_map<size_t, std::set<size_t>> epsClosures;

    result.start = nfa.start;

    // Compute epsilon-closures for all states
    for(const auto& [state, tr] : nfa.transitions){
        epsClosures[state] = epsClosure(nfa, state);
    }

    // Build new transitions based on epsilon-closures
    for(const auto & [state, closure] : epsClosures)
    {
        // For each state in the epsilon-closure, add its transitions
        for(const auto c : closure)
        {
            auto it = nfa.transitions.find(c);
            if(it != nfa.transitions.end()) {
                for (const auto & tr: it->second) {
                    if (tr.symbol != -1)
                    {
                        result.transitions[state].push_back({tr.to, tr.symbol});
                    }
                }
            }
            if(nfa.finishes.contains(c))
                result.finishes.insert(state);

        }
    }

    // Ensure finish states from original NFA are included
    for(const auto & finish : nfa.finishes)
    {
        if(!result.finishes.contains(finish))
            result.finishes.insert(finish);
    }

    return result;
}


//Simulate NFA on input word using DFS
bool simulateNFA(const NFA& nfa, const Word& word)
{
    //stack of pairs (state, index in word)
    std::stack<std::pair<size_t, size_t>> s;
    s.push({nfa.start, 0});

    //set of visited (state, index) pairs to avoid cycles
    std::set<std::pair<size_t,size_t>> visited;

    while(!s.empty()){
        auto [state, index] = s.top();
        s.pop();

        if(nfa.finishes.contains(state) && index == word.size()){
            return true;
        }

        if(visited.contains({state, index})){
            continue;
        }

        visited.insert({state, index});

        auto it = nfa.transitions.find(state);
        if (it == nfa.transitions.end()) continue;

        for(const auto & tr : it->second){
            if(tr.symbol == -1){
                s.push({tr.to, index});
            } else {
                if(index < word.size() && tr.symbol == word[index]){
                    s.push({tr.to, index + 1});
                }
            }
        }
    }

    return false;
}


/* Check which words match the regex
 * returns set of matching words
 */
std::set<size_t> wordsMatch(const regexp::RegExp& regexp, const std::vector<Word>& words)
{
    std::set<size_t> result;
    NFA thompson = buildNFA(regexp, 0); //Build NFA using Thompson's construction
    NFA nfa = epsilonRemover(thompson); //Remove epsilon transitions

    for(size_t i = 0; i < words.size(); i++){ //Simulate NFA on each word
        if(simulateNFA(nfa, words[i])){
            result.insert(i);
        }
    }

    return result;
}

int main()
{
    regexp::RegExp re4 = std::make_unique<regexp::Symbol>('h');
    assert(wordsMatch(re4, {Word{'h'}}) == std::set<size_t>{0});

    // basic test 1
    regexp::RegExp re1 = std::make_unique<regexp::Iteration>(
            std::make_unique<regexp::Concatenation>(
                    std::make_unique<regexp::Concatenation>(
                            std::make_unique<regexp::Concatenation>(
                                    std::make_unique<regexp::Iteration>(
                                            std::make_unique<regexp::Alternation>(
                                                    std::make_unique<regexp::Symbol>('a'),
                                                    std::make_unique<regexp::Symbol>('b'))),
                                    std::make_unique<regexp::Symbol>('a')),
                            std::make_unique<regexp::Symbol>('b')),
                    std::make_unique<regexp::Iteration>(
                            std::make_unique<regexp::Alternation>(
                                    std::make_unique<regexp::Symbol>('a'),
                                    std::make_unique<regexp::Symbol>('b')))));

    auto  test = wordsMatch(re1, {Word{}});

    assert(wordsMatch(re1, {Word{}}) == std::set<size_t>{0});
    assert(wordsMatch(re1, {Word{'a', 'b'}}) == std::set<size_t>{0});
    assert(wordsMatch(re1, {Word{'a'}}) == std::set<size_t>{});
    assert(wordsMatch(re1, {Word{'a', 'a', 'a', 'a'}}) == std::set<size_t>{});
    assert(wordsMatch(re1, {Word{'a', 'a', 'a', 'c'}}) == std::set<size_t>{});
    assert(wordsMatch(re1, {Word{'a', 'a', 0x07, 'c'}}) == std::set<size_t>{});
    assert(wordsMatch(re1, {Word{'a', 'a', 'b'}}) == std::set<size_t>{0});
    assert(wordsMatch(re1, {Word{'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b'}}) == std::set<size_t>{0});
    assert((wordsMatch(re1, {Word{}, Word{'a', 'b'}, Word{'a'}, Word{'a', 'a', 'a', 'a'}, Word{'a', 'a', 'a', 'c'}, Word{'a', 'a', 0x07, 'c'}, Word{'a', 'a', 'b'}, Word{'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b', 'a', 'a', 'b'}}) == std::set<size_t>{0, 1, 6, 7}));

    // basic test 2
    regexp::RegExp re2 = std::make_unique<regexp::Concatenation>(
            std::make_unique<regexp::Concatenation>(
                    std::make_unique<regexp::Iteration>(
                            std::make_unique<regexp::Concatenation>(
                                    std::make_unique<regexp::Concatenation>(
                                            std::make_unique<regexp::Iteration>(
                                                    std::make_unique<regexp::Alternation>(
                                                            std::make_unique<regexp::Symbol>('a'),
                                                            std::make_unique<regexp::Symbol>('b'))),
                                            std::make_unique<regexp::Iteration>(
                                                    std::make_unique<regexp::Alternation>(
                                                            std::make_unique<regexp::Symbol>('c'),
                                                            std::make_unique<regexp::Symbol>('d')))),
                                    std::make_unique<regexp::Iteration>(
                                            std::make_unique<regexp::Alternation>(
                                                    std::make_unique<regexp::Symbol>('e'),
                                                    std::make_unique<regexp::Symbol>('f'))))),
                    std::make_unique<regexp::Empty>()),
            std::make_unique<regexp::Iteration>(
                    std::make_unique<regexp::Alternation>(
                            std::make_unique<regexp::Symbol>('a'),
                            std::make_unique<regexp::Symbol>('b'))));
    assert(wordsMatch(re2, {Word{}}) == std::set<size_t>{});
    assert(wordsMatch(re2, {Word{'a', 'b'}}) == std::set<size_t>{});
    assert(wordsMatch(re2, {Word{'a', 'b', 'c', 'd'}}) == std::set<size_t>{});
    assert(wordsMatch(re2, {Word{'a', 'b', 'c', 'd', 'e', 'f'}}) == std::set<size_t>{});
    assert(wordsMatch(re2, {Word{'a', 'b', 'c', 'd', 'e', 'f', 'a', 'b'}}) == std::set<size_t>{});
    assert((wordsMatch(re2, {Word{}, Word{'a', 'b'}, Word{'a', 'b', 'c', 'd'}, Word{'a', 'b', 'c', 'd', 'e', 'f'}, Word{'a', 'b', 'c', 'd', 'e', 'f', 'a', 'b'}}) == std::set<size_t>{}));

    // basic test 3
    regexp::RegExp re3 = std::make_unique<regexp::Concatenation>(
            std::make_unique<regexp::Concatenation>(
                    std::make_unique<regexp::Concatenation>(
                            std::make_unique<regexp::Symbol>('0'),
                            std::make_unique<regexp::Symbol>('1')),
                    std::make_unique<regexp::Symbol>('1')),
            std::make_unique<regexp::Iteration>(
                    std::make_unique<regexp::Alternation>(
                            std::make_unique<regexp::Alternation>(
                                    std::make_unique<regexp::Concatenation>(
                                            std::make_unique<regexp::Concatenation>(
                                                    std::make_unique<regexp::Symbol>('0'),
                                                    std::make_unique<regexp::Symbol>('1')),
                                            std::make_unique<regexp::Symbol>('1')),
                                    std::make_unique<regexp::Concatenation>(
                                            std::make_unique<regexp::Concatenation>(
                                                    std::make_unique<regexp::Symbol>('1'),
                                                    std::make_unique<regexp::Iteration>(
                                                            std::make_unique<regexp::Symbol>('0'))),
                                            std::make_unique<regexp::Symbol>('1'))),
                            std::make_unique<regexp::Symbol>('0'))));
    assert(wordsMatch(re3, {Word{'0', '1'}}) == std::set<size_t>{});
    assert(wordsMatch(re3, {Word{'0', '1', '1'}}) == std::set<size_t>{0});
    assert(wordsMatch(re3, {Word{'0', '1', '1', '0'}}) == std::set<size_t>{0});
    assert(wordsMatch(re3, {Word{'0', '1', '1', '0', '1', '1', '1', '0', '0', '0'}}) == std::set<size_t>{});
    assert(wordsMatch(re3, {Word{'0', '1', '1', '0', '1', '1', '1', '0', '0', '1'}}) == std::set<size_t>{0});
    assert(wordsMatch(re3, {Word{'0', '1', '1', '0', '1', '1', '1', '0', '0', '1', '0'}}) == std::set<size_t>{0});
    assert((wordsMatch(re3, {Word{'0', '1'}, Word{'0', '1', '1'}, Word{'0', '1', '1', '0'}, Word{'0', '1', '1', '0', '1', '1', '1', '0', '0', '0'}, Word{'0', '1', '1', '0', '1', '1', '1', '0', '0', '1'}, Word{'0', '1', '1', '0', '1', '1', '1', '0', '0', '1', '0'}}) == std::set<size_t>{1, 2, 4, 5}));

    std::cout << "All tests passed\n";
}

