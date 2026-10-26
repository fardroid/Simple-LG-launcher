// Tests for reading an iCal feed and the calendar card's day logic (src/lib/ics.ts, src/lib/calendar.ts).   Run: npm test
// The texts are checked in Russian (the screens speak the TV's language).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'ru-RU', languages: ['ru-RU']}, configurable: true});
process.env.TZ = 'Asia/Baku';           // a fixed zone (UTC+4, no daylight saving), so the expected times never depend on the machine
import {build} from 'esbuild';
const dir = new URL('../node_modules/', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/calendar.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: dir + '.cache-calendar-test.mjs', platform: 'node', logLevel: 'error'});
await build({entryPoints: [new URL('../src/lib/ics.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: dir + '.cache-ics-test.mjs', platform: 'node', logLevel: 'error'});
const c = await import(dir + '.cache-calendar-test.mjs');
const ics = await import(dir + '.cache-ics-test.mjs');
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

const hhmm = (ms) => { const d = new Date(ms); return ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2); };
const ymd = (ms) => { const d = new Date(ms); return d.getFullYear() + '-' + ('0' + (d.getMonth() + 1)).slice(-2) + '-' + ('0' + d.getDate()).slice(-2); };
const at = (y, m, d, h = 0, mi = 0) => new Date(y, m - 1, d, h, mi).getTime();

const feed = [
  'BEGIN:VCALENDAR', 'VERSION:2.0',
  // a single timed event in UTC (14:00Z = 18:00 in Baku), with an escaped comma, a folded line and an alarm inside
  'BEGIN:VEVENT', 'UID:one@x', 'DTSTART:20261024T140000Z', 'DTEND:20261024T153000Z', 'SUMMARY:Тренировка\\, зал', 'LOCATION:Улица ', ' Нефтчиляр', 'BEGIN:VALARM', 'TRIGGER:-PT10M', 'ACTION:DISPLAY', 'END:VALARM', 'END:VEVENT',
  // weekly on Monday and Thursday at 10:00 Baku time, from 2026-10-01, one Monday skipped, one moved
  'BEGIN:VEVENT', 'UID:weekly@x', 'DTSTART;TZID=Asia/Baku:20261001T100000', 'DTEND;TZID=Asia/Baku:20261001T110000', 'RRULE:FREQ=WEEKLY;BYDAY=MO,TH', 'EXDATE;TZID=Asia/Baku:20261026T100000', 'SUMMARY:Планёрка', 'END:VEVENT',
  'BEGIN:VEVENT', 'UID:weekly@x', 'RECURRENCE-ID;TZID=Asia/Baku:20261029T100000', 'DTSTART;TZID=Asia/Baku:20261029T160000', 'DTEND;TZID=Asia/Baku:20261029T170000', 'SUMMARY:Планёрка (перенос)', 'END:VEVENT',
  // all day, two days
  'BEGIN:VEVENT', 'UID:trip@x', 'DTSTART;VALUE=DATE:20261027', 'DTEND;VALUE=DATE:20261029', 'SUMMARY:Командировка', 'END:VEVENT',
  // cancelled
  'BEGIN:VEVENT', 'UID:gone@x', 'DTSTART:20261024T100000Z', 'DTEND:20261024T110000Z', 'STATUS:CANCELLED', 'SUMMARY:Отменено', 'END:VEVENT',
  // daily, 3 times
  'BEGIN:VEVENT', 'UID:daily@x', 'DTSTART;TZID=Asia/Baku:20261024T080000', 'DTEND;TZID=Asia/Baku:20261024T083000', 'RRULE:FREQ=DAILY;COUNT=3', 'SUMMARY:Зарядка', 'END:VEVENT',
  // monthly, second Tuesday
  'BEGIN:VEVENT', 'UID:monthly@x', 'DTSTART;TZID=Asia/Baku:20260901T190000', 'DTEND;TZID=Asia/Baku:20260901T200000', 'RRULE:FREQ=MONTHLY;BYDAY=2TU', 'SUMMARY:Клуб', 'END:VEVENT',
  // yearly birthday, all day
  'BEGIN:VEVENT', 'UID:bd@x', 'DTSTART;VALUE=DATE:20100105', 'DTEND;VALUE=DATE:20100106', 'RRULE:FREQ=YEARLY', 'SUMMARY:День рождения', 'END:VEVENT',
  'END:VCALENDAR'
].join('\r\n');

const from = at(2026, 10, 20), to = at(2026, 11, 20);
const all = ics.expandIcs(feed, from, to);
const on = (title) => all.filter((e) => e.title === title).map((e) => ymd(e.start) + ' ' + hhmm(e.start) + '-' + hhmm(e.end));

eq(on('Тренировка, зал'), ['2026-10-24 18:00-19:30'], 'UTC time becomes local time; escaped comma is unescaped');
eq(all.find((e) => e.title === 'Тренировка, зал').place, 'Улица Нефтчиляр', 'a folded line is joined');
eq(on('Планёрка'), ['2026-10-22 10:00-11:00', '2026-11-02 10:00-11:00', '2026-11-05 10:00-11:00', '2026-11-09 10:00-11:00', '2026-11-12 10:00-11:00', '2026-11-16 10:00-11:00', '2026-11-19 10:00-11:00'], 'weekly Monday/Thursday; the skipped Monday and the moved Thursday are gone');
eq(on('Планёрка (перенос)'), ['2026-10-29 16:00-17:00'], 'the moved occurrence appears at its new time');
eq(all.filter((e) => e.title === 'Командировка').map((e) => [ymd(e.start), ymd(e.end), e.allDay]), [['2026-10-27', '2026-10-29', true]], 'an all-day event spans whole days');
eq(on('Отменено'), [], 'cancelled events are left out');
eq(on('Зарядка'), ['2026-10-24 08:00-08:30', '2026-10-25 08:00-08:30', '2026-10-26 08:00-08:30'], 'COUNT limits a daily repeat');
eq(on('Клуб'), ['2026-10-13 19:00-20:00'].filter(() => false).concat(['2026-11-10 19:00-20:00']), 'monthly on the second Tuesday (the October one is before the range)');
eq(all.filter((e) => e.title === 'День рождения').length, 0, 'a yearly event outside the range is not listed');
const jan = ics.expandIcs(feed, at(2027, 1, 1), at(2027, 1, 31)).filter((e) => e.title === 'День рождения').map((e) => ymd(e.start));
eq(jan, ['2027-01-05'], 'a yearly all-day event repeats');
eq(ics.expandIcs('garbage', from, to), [], 'garbage gives no events');

// ---- the card ----
const now = at(2026, 10, 24, 18, 30);
const part = c.parseCalendar('X-WR-CALNAME:Работа\r\n' + feed.replace('BEGIN:VCALENDAR\r\n', 'BEGIN:VCALENDAR\r\nX-WR-CALNAME:Работа\r\n'), 'https://example.com/a.ics', '', 0, now);
const second = c.parseCalendar('BEGIN:VCALENDAR\r\nX-WR-CALNAME:Семья\r\nBEGIN:VEVENT\r\nUID:s\r\nDTSTART:20261024T150000Z\r\nDTEND:20261024T160000Z\r\nSUMMARY:Ужин\r\nEND:VEVENT\r\nEND:VCALENDAR', '/media/developer/cals/family.ics', '12-34', 1, now);
const data = c.joinCalendars('k', [part, second], now);
eq([part.source.name, second.source.name, data.sources.length], ['Работа', 'Семья', 2], 'calendar names are read and the calendars are joined');
eq(data.events.filter((e) => e.src === 1).map((e) => e.title), ['Ужин'], 'each event remembers its calendar');
eq(c.dotsOfDay(data, c.startOfDay(now, 0)), [0, 1], 'dots: one per calendar that has events that day');
eq(c.parseCalendar('<html>nope</html>', 'u', '', 0, now), null, 'something that is not a calendar is rejected');
const today = c.eventsOfDay(data, c.startOfDay(now, 0), now, '24h');
eq(today.map((e) => [e.time, e.title, e.state]), [['08:00', 'Зарядка', 'past'], ['18:00', 'Тренировка, зал', 'now'], ['19:00', 'Ужин', 'next']], 'today: the morning is past, the workout is on now, dinner is next');
eq(today[1].note, 'сейчас', 'the running event says so');
eq(c.eventsOfDay(data, c.startOfDay(now, 3), now, '24h').map((e) => [e.time, e.title]), [['Весь день', 'Командировка']], 'all-day events are listed first with "Весь день"');
eq(c.eventsOfDay(data, c.startOfDay(now, -1), now, '24h'), [], 'a day without events is empty');
eq(c.eventsOfDay(data, c.startOfDay(now, 1), now, '24h')[0].state, 'next', 'a future day is not dimmed');
eq([c.relativeDay(0), c.relativeDay(1), c.relativeDay(-1), c.relativeDay(5), c.relativeDay(-3), c.relativeDay(21)], ['Сегодня', 'Завтра', 'Вчера', 'Через 5 дней', '3 дня назад', 'Через 21 день'], 'relative day words');
eq(c.dayHeading(now, 0), 'Суббота, 24 октября', 'the day heading');
eq(c.normalizeUrl('webcal://calendar.google.com/a b.ics'), 'https://calendar.google.com/ab.ics', 'webcal becomes https, spaces are dropped');
eq([c.validPath('/media/developer/cals/a.ics'), c.validPath('/media/developer/cals/'), c.validPath('/a/../b'), c.validPath("/a b; rm"), c.validSource('/x/y.ics'), c.validSource('rm -rf')], [true, true, false, false, true, false], 'files and folders on the TV: plain characters only');
eq([c.validUrl('https://calendar.google.com/calendar/ical/x%40gmail.com/private-abc/basic.ics'), c.validUrl("https://a.com/'; rm -rf /"), c.validUrl('ftp://x'), c.validUrl('javascript:alert(1)')], [true, false, false, false], 'only plain web addresses are accepted');

// ---- the month ----
const m = c.monthView(data, now, 0, c.startOfDay(now, 0));
eq([m.name, m.year, m.weeks.length, m.weeks[0].length], ['Октябрь', 2026, 6, 7], 'a month is 6 weeks of 7 days, with its name and year');
eq([m.weeks[0][0].day, m.weeks[0][0].inMonth, m.weeks[0][3].day], [28, false, 1], 'the grid starts on the Monday before the 1st (28 September)');
const cell = (day) => m.weeks.reduce((a, w) => a.concat(w), []).find((x) => x.inMonth && x.day === day);
eq([cell(24).today, cell(24).selected, cell(24).dots, cell(20).dots], [true, true, [0, 1], []], 'today and the selected day are marked, and the days with events carry the calendars\' dots');
eq(cell(27).dots.length + cell(28).dots.length, 2, 'a two-day all-day event gives a dot to each of its days (27 and 28)');
eq([c.monthOffsetOfDay(now, 0), c.monthOffsetOfDay(now, 10), c.monthOffsetOfDay(now, -30), c.monthOffsetOfDay(now, 186)], [0, 1, -1, 6], 'month offsets of days');
eq(c.dayOffsetOfMonth(now, 1), 8, 'the first day of next month is 8 days away');
eq(c.monthView(data, now, 1, 0).name, 'Ноябрь', 'the next month');

console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
