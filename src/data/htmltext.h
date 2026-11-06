// Text out of a page's HTML, without a DOM (lib/htmltext.ts): the page as lines of text, the items of its top-level lists, or the
// text of the elements with a given class. Built for Wikiquote's pages; good enough for the bits of HTML in news feeds too.
#pragma once

#include <string>
#include <vector>

/** "&amp;" → "&", "&#8212;" → "—" and so on. */
std::string decode_entities (const std::string &text);
/** The page as lines of text (one per block element), empty lines left out. `tables`: keep what is in tables. */
std::vector<std::string> html_lines (const std::string &html, bool tables = false);
/** Plain text of a bit of HTML: tags gone, entities decoded, blocks on lines of their own. */
std::string html_to_text (const std::string &html);

struct ListItem { std::string text, section; };
/** The items of the page's top-level lists, each with the headings of its section ("Quotes / Letters"). */
std::vector<ListItem> top_level_items (const std::string &html);
/** The text of every element whose class list has `name`. `tables`: look inside tables too. */
std::vector<ListItem> text_of_class (const std::string &html, const std::string &name, bool tables = false);
/** Text that collapses runs of white space (JavaScript's \s, no-break spaces included) into one space. */
std::string squeeze_spaces (const std::string &s);
