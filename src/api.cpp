#include "pch.h"
#include <dplyr/main.h>

#include <boost/scoped_ptr.hpp>

#include <tools/hash.h>
#include <tools/match.h>

#include <dplyr/CharacterVectorOrderer.h>

#include <dplyr/tbl_cpp.h>
#include <dplyr/visitor_impl.h>

#include <dplyr/JoinVisitor.h>

#include <dplyr/Result/Result.h>

#include <dplyr/DataFrameJoinVisitors.h>

#include <dplyr/bad.h>

namespace dplyr {

DataFrameVisitors::DataFrameVisitors(const Rcpp::DataFrame& data_) :
  data(data_),
  visitors(),
  visitor_names(vec_names_or_empty(data))
{

  for (int i = 0; i < data.size(); i++) {
    VectorVisitor* v = visitor(data[i]);
    visitors.push_back(v);
  }
}

DataFrameVisitors::DataFrameVisitors(const DataFrame& data_, const SymbolVector& names) :
  data(data_),
  visitors(),
  visitor_names(names)
{

  int n = names.size();
  CharacterVector data_names = vec_names_or_empty(data);
  IntegerVector indices = names.match_in_table(data_names);

  for (int i = 0; i < n; i++) {
    if (indices[i] == NA_INTEGER) {
      bad_col(names[i], "is unknown");
    }
    SEXP column = data[indices[i] - 1];
    visitors.push_back(visitor(column));
  }

}

DataFrameJoinVisitors::DataFrameJoinVisitors(const DataFrame& left_, const DataFrame& right_, const SymbolVector& names_left, const SymbolVector& names_right, bool warn_, bool na_match) :
  left(left_), right(right_),
  visitor_names_left(names_left),
  visitor_names_right(names_right),
  nvisitors(names_left.size()),
  visitors(nvisitors),
  warn(warn_)
{
  IntegerVector indices_left  = names_left.match_in_table(RCPP_GET_NAMES(left));
  IntegerVector indices_right = names_right.match_in_table(RCPP_GET_NAMES(right));

  for (int i = 0; i < nvisitors; i++) {
    const SymbolString& name_left  = names_left[i];
    const SymbolString& name_right = names_right[i];

    if (indices_left[i] == NA_INTEGER) {
      stop("'%s' column not found in lhs, cannot join", name_left.get_utf8_cstring());
    }
    if (indices_right[i] == NA_INTEGER) {
      stop("'%s' column not found in rhs, cannot join", name_right.get_utf8_cstring());
    }

    visitors[i] =
      join_visitor(
        Column(left[indices_left[i] - 1], name_left),
        Column(right[indices_right[i] - 1], name_right),
        warn, na_match
      );
  }
}

JoinVisitor* DataFrameJoinVisitors::get(int k) const {
  return visitors[k];
}

JoinVisitor* DataFrameJoinVisitors::get(const SymbolString& name) const {
  for (int i = 0; i < nvisitors; i++) {
    if (name == visitor_names_left[i]) return get(i);
  }
  stop("visitor not found for name '%s' ", name.get_utf8_cstring());
}

int DataFrameJoinVisitors::size() const {
  return nvisitors;
}

DataFrameSubsetVisitors::DataFrameSubsetVisitors(const Rcpp::DataFrame& data_) :
  data(data_),
  visitors(),
  visitor_names(vec_names_or_empty(data))
{
  for (int i = 0; i < data.size(); i++) {
    SubsetVectorVisitor* v = subset_visitor(data[i], visitor_names[i]);
    visitors.push_back(v);
  }
}

DataFrameSubsetVisitors::DataFrameSubsetVisitors(const DataFrame& data_, const SymbolVector& names) :
  data(data_),
  visitors(),
  visitor_names(names)
{

  CharacterVector data_names = vec_names_or_empty(data);
  IntegerVector indx = names.match_in_table(data_names);

  int n = indx.size();
  for (int i = 0; i < n; i++) {

    int pos = indx[i];
    if (pos == NA_INTEGER) {
      bad_col(names[i], "is unknown");
    }

    SubsetVectorVisitor* v = subset_visitor(data[pos - 1], data_names[pos - 1]);
    visitors.push_back(v);

  }

}

int DataFrameSubsetVisitors::size() const {
  return visitors.size();
}

SubsetVectorVisitor* DataFrameSubsetVisitors::get(int k) const {
  return visitors[k];
}

const SymbolString DataFrameSubsetVisitors::name(int k) const {
  return visitor_names[k];
}

int DataFrameSubsetVisitors::nrows() const {
  return data.nrows();
}

void DataFrameSubsetVisitors::structure(List& x, int nrows, CharacterVector classes) const {
  copy_most_attributes(x, data);
  set_class(x, classes);
  set_rownames(x, nrows);
  x.names() = visitor_names;
  copy_vars(x, data);
}

template <>
DataFrame DataFrameSubsetVisitors::subset(const LogicalVector& index, const CharacterVector& classes) const {
  const int n = index.size();
  std::vector<int> idx;
  idx.reserve(n);
  for (int i = 0; i < n; i++) {
    if (index[i] == TRUE) {
      idx.push_back(i);
    }
  }
  return subset(idx, classes);
}

CharacterVectorOrderer::CharacterVectorOrderer(const CharacterVector& data) :
  orders(no_init(data.size()))
{
  int n = data.size();
  if (n == 0) return;

  dplyr_hash_set<SEXP> set(n);

  // 1 - gather unique SEXP pointers from data
  SEXP* p_data = Rcpp::internal::r_vector_start<STRSXP>(data);
  SEXP previous = *p_data++;
  set.insert(previous);
  for (int i = 1; i < n; i++, p_data++) {
    SEXP s = *p_data;

    // we've just seen this string, keep going
    if (s == previous) continue;

    // is this string in the set already
    set.insert(s);
    previous = s;
  }

  // retrieve unique strings from the set
  int n_uniques = set.size();
  LOG_VERBOSE << "Sorting " <<  n_uniques << " unique character elements";

  CharacterVector uniques(set.begin(), set.end());
  CharacterVector s_uniques = Language("sort", uniques).fast_eval();

  // order the uniques with a callback to R
  IntegerVector o = r_match(uniques, s_uniques);

  // combine uniques and o into a hash map for fast retrieval
  dplyr_hash_map<SEXP, int> map(n_uniques);
  for (int i = 0; i < n_uniques; i++) {
    map.insert(std::make_pair(uniques[i], o[i]));
  }

  // grab min ranks
  p_data = Rcpp::internal::r_vector_start<STRSXP>(data);
  previous = *p_data++;

  int o_pos;
  orders[0] = o_pos = map.find(previous)->second;

  for (int i = 1; i < n; ++i, ++p_data) {
    SEXP s = *p_data;
    if (s == previous) {
      orders[i] = o_pos;
      continue;
    }
    previous = s;
    orders[i] = o_pos = map.find(s)->second;
  }

}

CharacterVector get_uniques(const CharacterVector& left, const CharacterVector& right) {
  int nleft = left.size(), nright = right.size();
  int n = nleft + nright;

  CharacterVector big = no_init(n);
  CharacterVector::iterator it = big.begin();
  std::copy(left.begin(), left.end(), it);
  std::copy(right.begin(), right.end(), it + nleft);
  return Language("unique", big).fast_eval();
}

}
