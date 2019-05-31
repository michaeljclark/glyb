#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <climits>

#include <vector>
#include <map>
#include <tuple>

#include "binpack.h"
#include "utf8.h"
#include "glyph.h"
#include "text.h"

/*
 * Text Part
 */

text_part::text_part(std::string s) : text(s) {}

text_part::text_part(std::string s, tag_map_t t) : text(s), tags(t) {}

text_part::text_part(std::string s, tags_initializer_t l) : text(s)
{
    for (auto li = l.begin(); li != l.end(); li++) {
        tags.insert(tags.end(), *li);
    }
}

std::string text_part::to_string()
{
    std::string s;
    for (auto i = tags.begin(); i != tags.end(); i++) {
        s.append("{");
        s.append(std::to_string(i->first));
        s.append("=");
        s.append(std::to_string(i->second));
        s.append(" ");
    }
    s.append("\"");
    s.append(text);
    s.append("\"");
    for (auto i = tags.begin(); i != tags.end(); i++) {
        s.append("}");
    }
    return s;
}

/*
 * Text Container
 */

text_container::text_container(std::string s) : parts({ text_part(s) }) {}

text_container::text_container(std::string s,
    tag_map_t t) : parts({ text_part(s, t) }) {}

text_container::text_container(std::string s,
    tags_initializer_t l) : parts({ text_part(s, l) }) {}

text_container::text_container(text_part c) : parts({ c }) {}

void text_container::erase(size_t offset, size_t count)
{
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end();) {
        size_t plen = i->text.size();
        size_t pbeg = (std::min)(plen, (std::max)(poff, offset) - poff);
        size_t pcnt = (std::min)(plen - pbeg, (size_t)(std::max)
            (ptrdiff_t(offset + count - poff - pbeg), ptrdiff_t(0)));
        if (offset < poff + plen) {
            if (pbeg == 0 && pcnt == plen) {
                i = parts.erase(i);
            } else {
                if (pcnt > 0) {
                    i->text.erase(pbeg, pcnt);
                }
                i++;
            }
        } else {
            i++;
        }
        poff += plen;
    }
    coalesce();
}

void text_container::insert(size_t offset, std::string s)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = (std::min)(plen, (std::max)(poff, offset) - poff);
        if (offset >= poff && offset <= poff + plen) {
            i->text.insert(pbeg, s);
            break;
        }
        poff += plen;
    }
    coalesce();
}

void text_container::insert(size_t offset, text_part c)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = (std::min)(plen, (std::max)(poff, offset) - poff);
        if (offset >= poff && offset <= poff + plen) {
            if (i->tags == c.tags) {
                i->text.insert(pbeg, c.text);
                break;
            } else {
                if (pbeg < plen) {
                    if (plen - pbeg > 0) {
                        std::string s1 = i->text.substr(0, pbeg);
                        std::string s2 = i->text.substr(pbeg, plen - pbeg);
                        i->text = s1;
                        parts.insert(std::next(parts.insert(std::next(i), c)),
                            text_part(s2, i->tags));
                    } else {
                        parts.insert(i, c);
                    }
                } else {
                    parts.insert(std::next(i), c);
                }
            }
            break;
        }
        poff += plen;
    }
    coalesce();
}

void text_container::append(std::string s)
{
    parts.insert(parts.end(), { s });
    coalesce();
}

void text_container::append(text_part c)
{
    parts.insert(parts.end(), c);
    coalesce();
}

void text_container::mark(size_t offset, size_t count, intptr_t attr,
    intptr_t val)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = (std::min)(plen, (std::max)(poff, offset) - poff);
        size_t pcnt = (std::min)(plen - pbeg, (size_t)(std::max)
            (ptrdiff_t(offset + count - poff - pbeg), ptrdiff_t(0)));
        if (pcnt == 0) {
            poff += plen;
            continue;
        }
        if (pbeg > 0) {
            if (pbeg + pcnt < plen) {
                // 3-way split (begin <S1> pbeg <S2> pbeg+pcnt <S3> end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg, pcnt); // <-- tags
                std::string s3 = i->text.substr(pbeg+pcnt);
                auto savetags1 = i->tags;
                auto savetags2 = i->tags;
                i->text = s1;
                savetags1[attr] = val;
                i = parts.insert(std::next(i), text_part(s2, savetags1));
                i = parts.insert(std::next(i), text_part(s3, savetags2));
            } else {
                // 2-way split (begin <S1> pbeg <S2> pbeg+pcnt/end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg); // <-- tags
                auto savetags = i->tags;
                i->text = s1;
                savetags[attr] = val;
                i = parts.insert(std::next(i), text_part(s2, savetags));
            }
        } else {
            if (pcnt < plen) {
                // 2-way split (begin/pbeg <S1> pbeg+pcnt <S2> end)
                std::string s1 = i->text.substr(0, pcnt); // <-- tags
                std::string s2 = i->text.substr(pcnt);
                auto savetags = i->tags;
                i->text = s1;
                i->tags[attr] = val;
                i = parts.insert(std::next(i), text_part(s2, savetags));
            } else {
                // whole string
                i->tags[attr] = val;
            }
        }
        poff += plen;
    }
    coalesce();
}

void text_container::unmark(size_t offset, size_t count, intptr_t attr)
{
    if (parts.size() == 0) {
        parts.insert(parts.end(),std::string());
    }
    size_t poff = 0;
    for (auto i = parts.begin(); i != parts.end(); i++) {
        size_t plen = i->text.size();
        size_t pbeg = (std::min)(plen, (std::max)(poff, offset) - poff);
        size_t pcnt = (std::min)(plen - pbeg, (size_t)(std::max)
            (ptrdiff_t(offset + count - poff - pbeg), ptrdiff_t(0)));
        if (pcnt == 0) {
            poff += plen;
            continue;
        }
        if (pbeg > 0) {
            if (pbeg + pcnt < plen) {
                // 3-way split (begin <S1> pbeg <S2> pbeg+pcnt <S3> end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg, pcnt); // <-- tags
                std::string s3 = i->text.substr(pbeg+pcnt);
                auto savetags1 = i->tags;
                auto savetags2 = i->tags;
                i->text = s1;
                savetags1.erase(attr);
                i = parts.insert(std::next(i), text_part(s2, savetags1));
                i = parts.insert(std::next(i), text_part(s3, savetags2));
            } else {
                // 2-way split (begin <S1> pbeg <S2> pbeg+pcnt/end)
                std::string s1 = i->text.substr(0, pbeg);
                std::string s2 = i->text.substr(pbeg); // <-- tags
                auto savetags = i->tags;
                i->text = s1;
                savetags.erase(attr);
                i = parts.insert(std::next(i), text_part(s2, savetags));
            }
        } else {
            if (pcnt < plen) {
                // 2-way split (begin/pbeg <S1> pbeg+pcnt <S2> end)
                std::string s1 = i->text.substr(0, pcnt); // <-- tags
                std::string s2 = i->text.substr(pcnt);
                auto savetags = i->tags;
                i->text = s1;
                i->tags.erase(attr);
                i = parts.insert(std::next(i), text_part(s2, savetags));
            } else {
                // whole string
                i->tags.erase(attr);
            }
        }
        poff += plen;
    }
    coalesce();
}

void text_container::coalesce()
{
    for (auto i = parts.rbegin(); i != parts.rend();) {
        if (std::next(i) != parts.rend() && std::next(i)->tags == i->tags) {
            std::next(i)->text.append(i->text);
            std::advance(i, 1);
            parts.erase(i.base());
        } else {
            std::advance(i, 1);
        }
    }
}

std::string text_container::as_plaintext()
{
    std::string s;
    for (const auto &piece : parts) {
        s += piece.text;
    }
    return s;
}

std::string text_container::to_string()
{
    std::string s;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i != 0) {
            s.append(" ");
        }
        s.append(parts[i].to_string());
    }
    return s;
}