#ifndef __CONFIG_EQUIVALENCES_H__
#define __CONFIG_EQUIVALENCES_H__

#pragma once


#include "equivalence.h"


static
std::vector<Equivalence> equivalences =
{
    {"1 little",            { {0}, {1}, {2}, {3} }},
    {"1 little + 1 big",    { {2,7}, {2,6}, {3,7}, {1,5}, {1,7}, {1,4}, {0,6}, {0,5}, {0,7},
                              {3,6}, {0,4}, {1,6}, {2,5}, {3,4}, {2,4}, {3,5} }},
    {"1 little + 2 big",    { {3,4,6}, {1,4,7}, {0,4,5}, {3,4,7}, {0,5,6}, {0,5,7}, {2,6,7},
                              {0,6,7}, {2,4,5}, {3,6,7}, {2,4,7}, {2,4,6}, {1,6,7}, {2,5,6},
                              {1,4,5}, {2,5,7}, {1,5,7}, {3,5,7}, {1,5,6}, {3,5,6}, {0,4,7},
                              {3,4,5}, {1,4,6}, {0,4,6} }},
    {"1 little + 3 big",    { {0,4,5,6}, {0,5,6,7}, {3,5,6,7}, {2,4,5,7}, {2,4,6,7}, {0,4,5,7},
                              {3,4,5,7}, {2,5,6,7}, {1,4,5,7}, {1,5,6,7}, {0,4,6,7}, {1,4,5,6},
                              {1,4,6,7}, {3,4,6,7}, {2,4,5,6}, {3,4,5,6} }},
    {"1 little + 4 big",    { {3,4,5,6,7}, {1,4,5,6,7}, {2,4,5,6,7}, {0,4,5,6,7} }},
    {"2 little",            { {0,1}, {1,2}, {1,3}, {2,3}, {0,3}, {0,2} }},
    {"2 little + 1 big",    { {2,3,5}, {1,2,7}, {1,2,4}, {2,3,6}, {2,3,7}, {0,1,4}, {0,1,5},
                              {0,1,6}, {0,1,7}, {0,3,7}, {0,2,5}, {0,2,4}, {0,2,7}, {0,2,6},
                              {0,3,6}, {1,2,6}, {1,2,5}, {1,3,7}, {0,3,4}, {1,3,6}, {0,3,5},
                              {1,3,5}, {1,3,4}, {2,3,4} }},
    {"2 little + 2 big",    { {1,2,4,6}, {0,1,6,7}, {0,2,4,6}, {1,2,4,7}, {0,2,4,5}, {0,1,5,6},
                              {1,3,4,5}, {2,3,4,5}, {0,3,4,6}, {2,3,4,6}, {1,3,5,6}, {1,2,5,7},
                              {1,3,6,7}, {1,2,6,7}, {0,3,6,7}, {1,3,4,7}, {2,3,5,7}, {0,2,5,6},
                              {0,1,4,5}, {0,3,5,7}, {0,1,4,6}, {0,3,4,7}, {0,2,6,7}, {2,3,4,7},
                              {1,3,5,7}, {1,2,5,6}, {2,3,5,6}, {0,2,4,7}, {2,3,6,7}, {0,2,5,7},
                              {0,1,5,7}, {0,3,5,6}, {0,1,4,7}, {1,3,4,6}, {0,3,4,5}, {1,2,4,5} }},
    {"2 little + 3 big",    { {1,2,5,6,7}, {1,3,5,6,7}, {2,3,4,6,7}, {0,1,4,6,7}, {0,2,5,6,7},
                              {1,2,4,5,7}, {0,3,4,5,6}, {0,1,4,5,6}, {1,3,4,5,6}, {2,3,4,5,6},
                              {0,2,4,5,7}, {1,3,4,6,7}, {0,1,5,6,7}, {0,3,4,6,7}, {2,3,5,6,7},
                              {0,2,4,6,7}, {1,2,4,6,7}, {0,3,4,5,7}, {0,1,4,5,7}, {1,2,4,5,6},
                              {2,3,4,5,7}, {0,2,4,5,6}, {0,3,5,6,7}, {1,3,4,5,7} }},
    {"2 little + 4 big",    { {0,3,4,5,6,7}, {0,2,4,5,6,7}, {1,3,4,5,6,7}, {1,2,4,5,6,7},
                              {0,1,4,5,6,7}, {2,3,4,5,6,7} }},
    {"3 little",            { {0,1,2}, {0,2,3}, {1,2,3}, {0,1,3} }},
    {"3 little + 1 big",    { {0,2,3,6}, {0,1,2,7}, {0,1,3,4}, {1,2,3,5}, {0,1,2,4}, {0,2,3,7},
                              {1,2,3,6}, {0,1,3,7}, {0,1,2,5}, {0,2,3,4}, {1,2,3,7}, {0,1,3,6},
                              {0,1,2,6}, {0,2,3,5}, {0,1,3,5}, {1,2,3,4} }},
    {"3 little + 2 big",    { {1,2,3,4,6}, {0,1,3,4,6}, {0,1,3,4,5}, {0,2,3,4,5}, {0,1,3,6,7},
                              {0,2,3,6,7}, {0,2,3,5,7}, {0,2,3,4,6}, {1,2,3,5,6}, {1,2,3,4,7},
                              {0,1,3,5,6}, {0,1,3,5,7}, {0,1,2,5,6}, {1,2,3,4,5}, {0,1,2,4,5},
                              {0,1,2,5,7}, {1,2,3,6,7}, {0,1,2,6,7}, {0,2,3,5,6}, {0,1,2,4,7},
                              {0,1,2,4,6}, {0,1,3,4,7}, {0,2,3,4,7}, {1,2,3,5,7} }},
    {"3 little + 3 big",    { {0,2,3,4,5,7}, {0,1,3,5,6,7}, {0,1,2,4,5,7}, {0,1,3,4,5,7},
                              {0,2,3,4,5,6}, {0,1,2,4,5,6}, {0,1,3,4,6,7}, {0,2,3,4,6,7},
                              {1,2,3,5,6,7}, {0,2,3,5,6,7}, {1,2,3,4,5,6}, {0,1,2,5,6,7},
                              {1,2,3,4,6,7}, {0,1,2,4,6,7}, {0,1,3,4,5,6}, {1,2,3,4,5,7} }},
    {"3 little + 4 big",    { {0,1,2,4,5,6,7}, {0,2,3,4,5,6,7}, {0,1,3,4,5,6,7}, {1,2,3,4,5,6,7} }},
    {"4 little",            { {0,1,2,3} }},
    {"4 little + 1 big",    { {0,1,2,3,4}, {0,1,2,3,5}, {0,1,2,3,6}, {0,1,2,3,7} }},
    {"4 little + 2 big",    { {0,1,2,3,4,7}, {0,1,2,3,4,6}, {0,1,2,3,6,7}, {0,1,2,3,4,5},
                              {0,1,2,3,5,6}, {0,1,2,3,5,7} }},
    {"4 little + 3 big",    { {0,1,2,3,4,5,7}, {0,1,2,3,4,6,7}, {0,1,2,3,4,5,6}, {0,1,2,3,5,6,7} }},
    {"4 little + 4 big",    { {0,1,2,3,4,5,6,7} }},
    {"1 big",               { {4}, {5}, {6}, {7} }},
    {"2 big",               { {4,7}, {6,7}, {4,6}, {4,5}, {5,7}, {5,6} }},
    {"3 big",               { {4,6,7}, {4,5,7}, {4,5,6}, {5,6,7} }},
    {"4 big",               { {4,5,6,7} }}
};

#endif /* __CONFIG_EQUIVALENCES_H__ */
