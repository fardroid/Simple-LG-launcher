// The news card (components/NewsCard.tsx): a page of headlines, each with its outlet and age; ← → turn the pages, ↑ ↓ choose a
// headline, OK opens it over the screen with its description and a QR code of its link (components/NewsDetail.tsx).
// And the quote of the day (components/QuoteCard.tsx): the text as big as the card allows, the author under it.
#pragma once

#include "data/feeds.h"
#include "data/quote.h"
#include "gfx/text.h"
#include "ui/boardlayout.h"

/** "12 min ago", "3 h ago" */
std::string age_text (long long at, long long now);
void draw_news (const BoardCard &card, const std::vector<FeedItem> *items, bool loading, int page, int selected, bool focused,
	int card_size, Face face, long long now);
/** The headline (its index in `items`) under the point x, y, as draw_news lays the page out; -1: none. */
int news_item_at (const BoardCard &card, const std::vector<FeedItem> *items, int page, int card_size, Face face, float x, float y);
/** The headline over the whole screen. `fade`: 0-1 as it appears. */
void draw_news_detail (const FeedItem &item, Face face, long long now, float fade);
void draw_quote (const BoardCard &card, const Quote *quote, bool loading, int card_size, Face face);
