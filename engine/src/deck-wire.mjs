// SPDX-License-Identifier: GPL-3.0-or-later
// Binary encoder adapted from StonedModder/Prospero-Menu.
import {invariant} from './errors.mjs';
export const DECK_WIRE = Object.freeze({ magic: 0x54414344, version: 14, bytes: 19024, rows: 32, rowBytes: 292, theme: 18776, logo: 18824, videoSerial: 19016 });
// ProsperoMenu: the menu has no Cheats or Saved games pages. Page 5 remains only
// for ProsperoMenu's separate dashboard cheats window.
const pages = ['Home', 'Payloads', 'Games', 'Transfers', 'Settings'];
// Longest prefix of a UTF-8 buffer that fits in `limit` bytes without
// splitting a code point.
function utf8Prefix(input, limit) {
  let length = Math.min(input.length, Math.max(0, limit));
  while (length > 0 && length < input.length && (input[length] & 0xc0) === 0x80) length--;
  return length;
}
function text(buffer, offset, bytes, value) {
  const input = Buffer.from(String(value ?? '').replace(/[\x00-\x1f\x7f]/g, ' '));
  let length = Math.min(input.length, bytes - 1);
  // Do not split a UTF-8 code point at a fixed-width wire boundary.
  while (length < input.length && length && (input[length] & 0xc0) === 0x80) length--;
  input.copy(buffer, offset, 0, length);
}
export function encodeDeck(frame) {
  invariant(frame.rows.length <= DECK_WIRE.rows, 'DECK_LIMIT', 'Too many deck rows.');
  const accent = ['mint', 'amber', 'ice'].indexOf(frame.accent ?? 'mint');
  invariant(accent >= 0, 'DECK_ACCENT', 'Unknown deck accent.');
  const out = Buffer.alloc(DECK_WIRE.bytes);
  out.writeUInt32LE(DECK_WIRE.magic, 0); out.writeUInt16LE(DECK_WIRE.version, 4);
  out.writeUInt16LE((frame.ready ? 1 : 0) | (frame.ipVisible ? 2 : 0) | (frame.canBack ? 4 : 0) | (accent << 3) | (frame.grid ? 32 : 0) | (frame.reducedMotion ? 64 : 0) | (frame.keyboard ? 128 : 0) | (frame.browser ? 256 : 0), 6);
  out.writeUInt32LE(frame.sequence, 8); out.writeUInt16LE(frame.page, 12); out.writeUInt16LE(frame.rows.length, 14);
  // ProsperoMenu windowing: the frame ships a moving window of
  // rows. 0xFFFF focus means the deck keeps its own focused item.
  out.writeUInt16LE(Math.max(0, frame.windowStart || 0), 16);
  out.writeUInt16LE(Math.min(65535, frame.total ?? frame.rows.length), 18);
  out.writeUInt16LE(frame.focus ?? 0xFFFF, 20);
  text(out, 24, 64, frame.title); text(out, 88, 160, frame.subtitle);
  text(out, 248, 160, frame.footer); text(out, 408, 64, frame.ip);
  frame.rows.forEach((row, index) => {
    const at = 472 + index * DECK_WIRE.rowBytes;
    // token is a full 32-byte hex identifier, not a C string.
    Buffer.from(row.token).copy(out, at, 0, 32);
    // ProsperoMenu v10 rows: a right-aligned value rides after the label's
    // terminator; headers are never enabled; adjustable rows take Left/Right.
    const header = row.kind === 'header', value = header || row.value == null || row.value === '' ? null : String(row.value);
    if (value !== null) {
      const v = Buffer.from(value.replace(/[\x00-\x1f\x7f]/g, ' '));
      const valueBytes = utf8Prefix(v, 31);
      const label = Buffer.from(String(row.label ?? '').replace(/[\x00-\x1f\x7f]/g, ' '));
      const labelBytes = utf8Prefix(label, 95 - valueBytes - 1);
      label.copy(out, at + 32, 0, labelBytes); v.copy(out, at + 32 + labelBytes + 1, 0, valueBytes);
    } else text(out, at + 32, 96, row.label);
    text(out, at + 128, 160, row.detail);
    out.writeUInt32LE(header ? 8 : (row.enabled ? 1 : 0) | (row.on ? 2 : 0) | (row.adjust && row.enabled ? 4 : 0) | (value !== null ? 16 : 0) | (row.triangle && row.enabled ? 32 : 0), at + 288);
  });
  out.writeUInt32LE(frame.metricsSequence || 1, 9816);text(out, 9820, 192, frame.metrics);
  const input = frame.input;
  if (input) {
    invariant(input.events.length <= 128, 'DECK_INPUT_LIMIT', 'Too many controller events.');
    for (const [offset, value] of [[10016, input.epoch], [10020, input.serial], [10024, input.active], [10028, input.captured ? 1 : 0], [10036, input.connection], [10040, input.events.length]]) out.writeUInt32LE(value, offset);
    out.writeInt32LE(input.owner, 10032);out.writeBigUInt64LE(BigInt(Math.floor(input.deadline)), 10048);
    out.writeBigUInt64LE(BigInt(Math.ceil(input.captureAt || 0)), 10056);
    input.events.forEach((e, i) => { const at = 10064 + i * 24;out.writeUInt32LE(e.buttons, at);out.writeInt32LE(e.handle, at + 4);
      out.writeUInt32LE(e.connection, at + 8);out.writeUInt32LE(e.sequence, at + 12);out.writeBigUInt64LE(BigInt(Math.floor(e.time)), at + 16); });
  }
  const notice = frame.notices?.current;
  out.writeUInt32LE(frame.notices?.sequence || 1, 13136);
  if (notice && notice.remainingMs > 0) {
    out.writeUInt32LE(Math.min(9000, Math.ceil(notice.remainingMs)), 13140);
    out.writeUInt32LE(notice.level === 'attention' ? 2 : 1, 13144);out.writeUInt32LE(notice.count, 13148);
    text(out, 13152, 96, notice.title + (notice.count > 1 ? ` (${notice.count})` : ''));
    text(out, 13248, 192, notice.detail);
  }
  if(frame.browser){
    invariant(frame.browser.panes.length===2&&frame.browser.panes.every(p=>p.entries.length===12),'DECK_BROWSER','Invalid browser panes.');
    out.writeUInt32LE(frame.browser.active,13440);
    frame.browser.panes.forEach((p,i)=>{text(out,13444+i*160,160,p.path);p.entries.forEach((r,j)=>{const at=13924+(i*12+j)*164;text(out,at,96,r.label);text(out,at+96,64,r.detail);out.writeUInt32LE((r.enabled?1:0)|(r.on?2:0)|(r.directory?4:0),at+160);});});
    text(out,13764,160,frame.browser.status);
  }
  const dashboard=frame.dashboard;
  if(dashboard){out.writeUInt32LE(dashboard.enabled?1:0,17864);out.writeUInt32LE(dashboard.epoch,17868);out.writeUInt32LE(dashboard.ack,17872);
    invariant(dashboard.items.length<=32,'DASHBOARD_LIMIT','Too many dashboard records.');out.writeUInt32LE(dashboard.items.length,17876);
    dashboard.items.forEach((g,i)=>{text(out,17880+i*28,10,g.titleId);text(out,17890+i*28,16,g.version);});}
  // ProsperoSettings theme: serial then RGBA8 colours (accent, background,
  // text, muted, separator, scrollbar, edge, focus fill). Serial 0 = built-in.
  const theme = frame.theme;
  if (theme?.serial) {
    invariant(Array.isArray(theme.colors) && theme.colors.length === 10, 'DECK_THEME', 'Invalid theme.');
    out.writeUInt32LE(theme.serial >>> 0, DECK_WIRE.theme);
    theme.colors.forEach((c, i) => out.writeUInt32LE(c >>> 0, DECK_WIRE.theme + 4 + i * 4));
  }
  // v12: the replaceable header logo. The deck loads it with the validated
  // image widget; an empty path keeps the drawn mark, so a host without the
  // image ABI still renders a header.
  const logo = frame.logoPath || '';
  if (logo) {
    invariant(typeof logo === 'string' && !/[\x00-\x1f\x7f]/.test(logo) && Buffer.byteLength(logo) < 192, 'DECK_LOGO', 'Invalid logo path.');
    out.write(logo, DECK_WIRE.logo, Buffer.byteLength(logo), 'utf8');
  }
  // v13: boot animation frame counter. The deck reloads the frame image
  // whenever this changes, giving frame-accurate animation.
  out.writeUInt32LE((frame.videoSerial || 0) >>> 0, DECK_WIRE.videoSerial);
  return out;
}
