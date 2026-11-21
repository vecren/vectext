module vectext:font.collection.android;
import :font.state;

namespace vectext {
using namespace rstd::prelude;
auto candidates(Collection &collection, TextRequest request) -> Result<Vec<Candidate>, TextError>;
auto candidate_font(Collection &collection, Candidate &candidate, Scalar size, bool bold = false)
    -> Result<Arc<Font>, TextError>;
auto candidate_supports(const Candidate &candidate, Index cp) -> bool;
auto fallback_candidate(Collection &collection, Vec<Candidate> &choices, TextRequest request,
                        Index cp) -> Result<Option<Candidate>, TextError>;
} // namespace vectext
