#ifndef __CSV_H__
#define __CSV_H__

#pragma once


#include "string_util.h"

#include <iostream>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>


template <bool> class Slice;
using RowSlice = Slice<true>;
using ColumnSlice = Slice<false>;

template <bool> class ConstSlice;
using ConstRowSlice = ConstSlice<true>;
using ConstColumnSlice = ConstSlice<false>;

namespace detail {

template <typename Outer>
class Iter
{
   private:
    using iterator = typename Outer::inner_iterator;
    using value_type = typename Outer::reference;

    Outer*      _outer;
    iterator    _i;

    Iter(Outer* outer, iterator i) :
        _outer{outer}, _i{i}
    {}

    friend Outer;

   public:
    Iter& operator++()
    {
        ++_i;
        return *this;
    }

    Iter operator++(int)
    {
        Iter o{*this};
        ++_i;
        return o;
    }

    bool operator==(const Iter& o) const
    {
        return _i == o._i;
    }

    bool operator!=(const Iter& o) const
    {
        return _i != o._i;
    }

    value_type operator*()
    {
        return _outer->get(*_i);
    }

    value_type* operator->()
    {
        return &_outer->get(*_i);
    }
};

template <typename Outer>
class ConstIter
{
   private:
    using iterator = typename Outer::inner_const_iterator;
    using value_type = typename Outer::value_type;

    const Outer*    _outer;
    iterator        _i;

    ConstIter(const Outer* outer, iterator i) :
        _outer{outer}, _i{i}
    {}

    friend Outer;
   public:
    ConstIter& operator++()
    {
        ++_i;
        return *this;
    }

    ConstIter operator++(int)
    {
        ConstIter o{*this};
        ++_i;
        return o;
    }

    bool operator==(const ConstIter& o) const
    {
        return _i == o._i;
    }

    bool operator!=(const ConstIter& o) const
    {
        return _i != o._i;
    }

    value_type operator*()
    {
        return _outer->get(*_i);
    }

    value_type* operator->()
    {
        return &_outer->get(*_i);
    }
};

} /* namespace detail */

class CSVData
{
   public:
    using reference = std::string&;
    using value_type = std::string;

   private:
    std::vector<std::string> _columns;
    std::vector<std::string> _rows;

    std::map<std::string, std::map<std::string, std::string>> _data;

   public:
    CSVData() = default;

    CSVData(const std::string& file, char sep=',',
            bool column_names=true, bool row_names=true) :
        _columns{}, _rows{}, _data{}
    {
        from_file(file, sep, column_names, row_names);
    }

    void from_file(const std::string& file, char sep=',',
            bool column_names=true, bool row_names=true)
    {
        std::ifstream f{file};
        if (!f.is_open())
            throw std::runtime_error{"Can't open file " + file + "."};

        std::string cur_line;
        bool columns_read = false;
        int row_nr = 0;
        while (std::getline(f, cur_line)) {
            auto elements = string_util::split(cur_line, sep);

            /* Read the column names if necessary otherwise enumerate them. */
            if (!columns_read) {
                columns_read = true;
                if (column_names) {
                    if (row_names) {
                        elements.erase(elements.begin());
                        _columns = elements;
                    } else {
                        _columns = elements;
                    }

                    continue;
                } else {
                    for (size_t i = 0; i < elements.size(); ++i)
                        _columns.push_back(std::to_string(i));
                }
            }

            auto ele = elements.begin();

            /* Get the current row name or if not specified enumerate it. */
            std::string row;
            if (row_names) {
                row = *ele;
                ++ele;
            } else {
                row = std::to_string(row_nr);
                ++row_nr;
            }
            _rows.push_back(row);

            /* Parse in data. */
            _data[row] = {};
            for (auto& col : _columns) {
                _data[row][col] = *ele;
                ++ele;
            }
        }
    }

    void to_file(const std::string& file, char sep=',',
            bool column_names=true, bool row_names=true)
    {
        std::ofstream f{file};
        if (!f.is_open())
            throw std::runtime_error{"Can't open file " + file + "."};

        if (column_names) {
            f << string_util::join(_columns, sep) << std::endl;
        }

        for (auto& row : _rows) {
            std::vector<std::string> elements;
            if (row_names)
                elements.push_back(row);

            for (auto& col : _columns)
                elements.push_back(_data[row][col]);

            f << string_util::join(elements) << std::endl;
        }
    }

    std::vector<std::string> columns() const
    {
        return _columns;
    }

    std::vector<std::string> rows() const
    {
        return _rows;
    }

    reference operator()(const std::string& row, const std::string& col)
    {
        return _data.at(row).at(col);
    }

    value_type operator()(const std::string& row, const std::string& col) const
    {
        return _data.at(row).at(col);
    }

    reference operator()(int row_nr, const std::string& col)
    {
        return _data.at(_rows.at(row_nr)).at(col);
    }

    value_type operator()(int row_nr, const std::string& col) const
    {
        return _data.at(_rows.at(row_nr)).at(col);
    }

    reference operator()(const std::string& row, int col_nr)
    {
        return _data.at(row).at(_columns.at(col_nr));
    }

    value_type operator()(const std::string& row, int col_nr) const
    {
        return _data.at(row).at(_columns.at(col_nr));
    }

    reference operator()(int row_nr, int col_nr)
    {
        return _data.at(_rows.at(row_nr)).at(_columns.at(col_nr));
    }

    value_type operator()(int row_nr, int col_nr) const
    {
        return _data.at(_rows.at(row_nr)).at(_columns.at(col_nr));
    }

    reference at(const std::string& row, const std::string& col)
    {
        return _data.at(row).at(col);
    }

    value_type at(const std::string& row, const std::string& col) const
    {
        return _data.at(row).at(col);
    }

    reference at(int row_nr, const std::string& col)
    {
        return _data.at(_rows.at(row_nr)).at(col);
    }

    value_type at(int row_nr, const std::string& col) const
    {
        return _data.at(_rows.at(row_nr)).at(col);
    }

    reference at(const std::string& row, int col_nr)
    {
        return _data.at(row).at(_columns.at(col_nr));
    }

    value_type at(const std::string& row, int col_nr) const
    {
        return _data.at(row).at(_columns.at(col_nr));
    }

    reference at(int row_nr, int col_nr)
    {
        return _data.at(_rows.at(row_nr)).at(_columns.at(col_nr));
    }

    value_type at(int row_nr, int col_nr) const
    {
        return _data.at(_rows.at(row_nr)).at(_columns.at(col_nr));
    }

    ColumnSlice row(const std::string& name);
    ConstColumnSlice row(const std::string& name) const;
    ColumnSlice row(int row_nr);
    ConstColumnSlice row(int row_nr) const;

    RowSlice column(const std::string& name);
    ConstRowSlice column(const std::string& name) const;
    RowSlice column(int col_nr);
    ConstRowSlice column(int col_nr) const;

   public:
    class RowIter
    {
       public:
        using iterator = detail::Iter<RowIter>;
        using const_iterator = detail::ConstIter<RowIter>;
        using inner_iterator = typename std::vector<std::string>::iterator;
        using inner_const_iterator = typename std::vector<std::string>::const_iterator;
        using reference = ColumnSlice;
        using value_type = ConstColumnSlice;

       private:
        CSVData*                    _data;
        std::vector<std::string>    _elements;

        RowIter(CSVData* data) :
            _data{data}, _elements{data->rows()}
        {}

        friend CSVData;

       private:
        reference get(const std::string& name);
        value_type get(const std::string& name) const;

        friend iterator;
        friend const_iterator;

       public:
        iterator begin()
        {
            return iterator{this, _elements.begin()};
        }

        const_iterator begin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        iterator end()
        {
            return iterator{this, _elements.end()};
        }

        const_iterator end() const
        {
            return const_iterator{this, _elements.end()};
        }

        const_iterator cbegin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        const_iterator cend() const
        {
            return const_iterator{this, _elements.end()};
        }
    };

    class ConstRowIter
    {
       public:
        using const_iterator = detail::ConstIter<ConstRowIter>;
        using inner_const_iterator = typename std::vector<std::string>::const_iterator;
        using value_type = ConstColumnSlice;

       private:
        const CSVData*              _data;
        std::vector<std::string>    _elements;

        ConstRowIter(const CSVData* data) :
            _data{data}, _elements{data->rows()}
        {}

        friend CSVData;

       private:
        value_type get(const std::string& name) const;

        friend const_iterator;

       public:
        const_iterator begin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        const_iterator end() const
        {
            return const_iterator{this, _elements.end()};
        }

        const_iterator cbegin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        const_iterator cend() const
        {
            return const_iterator{this, _elements.end()};
        }
    };

    class ColumnIter
    {
       public:
        using iterator = detail::Iter<ColumnIter>;
        using const_iterator = detail::ConstIter<ColumnIter>;
        using inner_iterator = typename std::vector<std::string>::iterator;
        using inner_const_iterator = typename std::vector<std::string>::const_iterator;
        using reference = RowSlice;
        using value_type = ConstRowSlice;

       private:
        CSVData*                    _data;
        std::vector<std::string>    _elements;

        ColumnIter(CSVData* data) :
            _data{data}, _elements{data->columns()}
        {}

        friend CSVData;

       private:
        reference get(const std::string& name);
        value_type get(const std::string& name) const;

        friend iterator;
        friend const_iterator;

       public:
        iterator begin()
        {
            return iterator{this, _elements.begin()};
        }

        const_iterator begin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        iterator end()
        {
            return iterator{this, _elements.end()};
        }

        const_iterator end() const
        {
            return const_iterator{this, _elements.end()};
        }

        const_iterator cbegin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        const_iterator cend() const
        {
            return const_iterator{this, _elements.end()};
        }
    };

    class ConstColumnIter
    {
       public:
        using const_iterator = detail::ConstIter<ConstColumnIter>;
        using inner_const_iterator = typename std::vector<std::string>::const_iterator;
        using value_type = ConstRowSlice;

       private:
        const CSVData*              _data;
        std::vector<std::string>    _elements;

        ConstColumnIter(const CSVData* data) :
            _data{data}, _elements{data->columns()}
        {}

        friend CSVData;

       private:
        value_type get(const std::string& name) const;

        friend const_iterator;

       public:
        const_iterator begin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        const_iterator end() const
        {
            return const_iterator{this, _elements.end()};
        }

        const_iterator cbegin() const
        {
            return const_iterator{this, _elements.begin()};
        }

        const_iterator cend() const
        {
            return const_iterator{this, _elements.end()};
        }
    };

   public:
    using row_iterator = RowIter;
    using const_row_iterator = ConstRowIter;
    using column_iterator = ColumnIter;
    using const_column_iterator = ConstColumnIter;

    row_iterator row_iter()
    {
        return row_iterator{this};
    }

    const_row_iterator row_iter() const
    {
        return const_row_iterator{this};
    }

    const_row_iterator const_row_iter() const
    {
        return const_row_iterator{this};
    }

    column_iterator column_iter()
    {
        return column_iterator{this};
    }

    const_column_iterator column_iter() const
    {
        return const_column_iterator{this};
    }

    const_column_iterator const_column_iter() const
    {
        return const_column_iterator{this};
    }
};


template <bool row>
class Slice
{
   public:
    using iterator = detail::Iter<Slice>;
    using const_iterator = detail::ConstIter<Slice>;
    using inner_iterator = typename std::vector<std::string>::iterator;
    using inner_const_iterator = typename std::vector<std::string>::const_iterator;
    using reference = typename CSVData::reference;
    using value_type = typename CSVData::value_type;

   private:
    CSVData*                    _data;
    std::string                 _fixed;
    std::vector<std::string>    _other_dim;

    Slice(CSVData* data, const std::string& fixed) :
        _data{data}, _fixed{fixed}, _other_dim{}
    {
        if (row)
            _other_dim = _data->rows();
        else
            _other_dim = _data->columns();
    }

    friend CSVData;

   private:
    reference get(const std::string& name)
    {
        return at(name);
    }

    value_type get(const std::string& name) const
    {
        return at(name);
    }

    friend iterator;
    friend const_iterator;

   public:
    Slice(const Slice&) = default;
    Slice(Slice&&) = default;

    ~Slice() = default;

    Slice& operator=(const Slice&) = default;
    Slice& operator=(Slice&&) = default;

    std::string fixed() const
    {
        return _fixed;
    }

    std::vector<std::string> names() const
    {
        return _other_dim;
    }

    reference at(const std::string& name)
    {
        if (row)
            return _data->at(name, _fixed);
        else
            return _data->at(_fixed, name);
    }

    value_type at(const std::string& name) const
    {
        if (row)
            return _data->at(name, _fixed);
        else
            return _data->at(_fixed, name);
    }

    reference at(int nr)
    {
        if (row)
            return _data->at(nr, _fixed);
        else
            return _data->at(_fixed, nr);
    }

    value_type at(int nr) const
    {
        if (row)
            return _data->at(nr, _fixed);
        else
            return _data->at(_fixed, nr);
    }

    reference operator()(const std::string& name)
    {
        return at(name);
    }

    value_type operator()(const std::string& name) const
    {
        return at(name);
    }

    reference operator()(int nr)
    {
        return at(nr);
    }

    value_type operator()(int nr) const
    {
        return at(nr);
    }

    iterator begin()
    {
        return iterator{this, _other_dim.begin()};
    }

    const_iterator begin() const
    {
        return const_iterator{this, _other_dim.begin()};
    }

    iterator end()
    {
        return iterator{this, _other_dim.end()};
    }

    const_iterator end() const
    {
        return const_iterator{this, _other_dim.end()};
    }

    const_iterator cbegin() const
    {
        return const_iterator{this, _other_dim.begin()};
    }

    const_iterator cend() const
    {
        return const_iterator{this, _other_dim.end()};
    }
};

template <bool row>
class ConstSlice
{
   public:
    using const_iterator = detail::ConstIter<ConstSlice>;
    using inner_const_iterator = typename std::vector<std::string>::iterator;
    using value_type = typename CSVData::value_type;

   private:
    const CSVData*              _data;
    std::string                 _fixed;
    std::vector<std::string>    _other_dim;

    ConstSlice(const CSVData* data, const std::string& fixed) :
        _data{data}, _fixed{fixed}, _other_dim{}
    {
        if (row)
            _other_dim = _data->rows();
        else
            _other_dim = _data->columns();
    }

    friend CSVData;

   private:
    value_type get(const std::string& name) const
    {
        return at(name);
    }

    friend const_iterator;

   public:
    ConstSlice(const ConstSlice&) = default;
    ConstSlice(ConstSlice&&) = default;

    ~ConstSlice() = default;

    ConstSlice& operator=(const ConstSlice&) = default;
    ConstSlice& operator=(ConstSlice&&) = default;

    std::string fixed() const
    {
        return _fixed;
    }

    std::vector<std::string> names() const
    {
        return _other_dim;
    }

    value_type at(const std::string& name) const
    {
        if (row)
            return _data->at(name, _fixed);
        else
            return _data->at(_fixed, name);
    }

    value_type at(int nr) const
    {
        if (row)
            return _data->at(nr, _fixed);
        else
            return _data->at(_fixed, nr);
    }

    value_type operator()(const std::string& name) const
    {
        return at(name);
    }

    value_type operator()(int nr) const
    {
        return at(nr);
    }

    const_iterator begin() const
    {
        return const_iterator{this, _other_dim.begin()};
    }

    const_iterator end() const
    {
        return const_iterator{this, _other_dim.end()};
    }

    const_iterator cbegin() const
    {
        return const_iterator{this, _other_dim.begin()};
    }

    const_iterator cend() const
    {
        return const_iterator{this, _other_dim.end()};
    }
};


ColumnSlice CSVData::row(const std::string& name)
{
    return ColumnSlice{this, name};
}

ConstColumnSlice CSVData::row(const std::string& name) const
{
    return ConstColumnSlice{this, name};
}

ColumnSlice CSVData::row(int row_nr)
{
    return ColumnSlice{this, _rows.at(row_nr)};
}

ConstColumnSlice CSVData::row(int row_nr) const
{
    return ConstColumnSlice{this, _rows.at(row_nr)};
}

RowSlice CSVData::column(const std::string& name)
{
    return RowSlice{this, name};
}

ConstRowSlice CSVData::column(const std::string& name) const
{
    return ConstRowSlice{this, name};
}

RowSlice CSVData::column(int col_nr)
{
    return RowSlice{this, _columns.at(col_nr)};
}

ConstRowSlice CSVData::column(int col_nr) const
{
    return ConstRowSlice{this, _columns.at(col_nr)};
}


CSVData::RowIter::reference CSVData::RowIter::get(const std::string& name)
{
    return _data->row(name);
}

CSVData::RowIter::value_type CSVData::RowIter::get(const std::string& name) const
{
    return const_cast<const CSVData*>(_data)->row(name);
}

CSVData::ConstRowIter::value_type CSVData::ConstRowIter::get(const std::string& name) const
{
    return _data->row(name);
}

CSVData::ColumnIter::reference CSVData::ColumnIter::get(const std::string& name)
{
    return _data->column(name);
}

CSVData::ColumnIter::value_type CSVData::ColumnIter::get(const std::string& name) const
{
    return const_cast<const CSVData*>(_data)->column(name);
}

CSVData::ConstColumnIter::value_type CSVData::ConstColumnIter::get(const std::string& name) const
{
    return _data->column(name);
}

#endif /* __CSV_H__ */
