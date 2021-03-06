% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/utils-tidy-eval.R
\name{tidyeval}
\alias{tidyeval}
\alias{quo}
\alias{quos}
\alias{enquo}
\alias{enquos}
\alias{quo_name}
\alias{sym}
\alias{ensym}
\alias{syms}
\alias{ensyms}
\alias{expr}
\alias{exprs}
\alias{enexpr}
\alias{enexprs}
\alias{.data}
\title{Tidy eval helpers}
\description{
\itemize{
\item \code{\link[rlang]{sym}()} creates a symbol from a string and
\code{\link[rlang]{syms}()} creates a list of symbols from a
character vector.
\item \code{\link[rlang]{expr}()} and \code{\link[rlang]{quo}()} quote
one expression. \code{quo()} wraps the quoted expression in a quosure.

The plural variants \code{\link[rlang]{exprs}()} and
\code{\link[rlang]{quos}()} return a list of quoted expressions or
quosures.
\item \code{\link[rlang]{enexpr}()} and \code{\link[rlang]{enquo}()}
capture the expression supplied as argument by the user of the
current function (\code{enquo()} wraps this expression in a quosure).

\code{\link[rlang]{enexprs}()} and \code{\link[rlang]{enquos}()}
capture multiple expressions supplied as arguments, including
\code{...}.
}
}
\keyword{internal}
