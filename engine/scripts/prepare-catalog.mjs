// SPDX-License-Identifier: GPL-3.0-or-later
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'), source=process.env.PROSPEROMENU_SQLITE_LIB || path.join(root,'../etahen-13.60/dependencies/lib/libsqlite3.a');
const data=await readFile(source);
if(data.length!==7576386||createHash('sha256').update(data).digest('hex')!=='c40f492901fe1d4f86e632ae79e08bb48ea3a6e1e58dbc9935efbe80451f6fd4')throw Error('SQLite PS5 library differs from reviewed build input');
await mkdir(path.join(root,'build/catalog'),{recursive:true});await writeFile(path.join(root,'build/catalog/libsqlite3.a'),data);
await writeFile(path.join(root,'build/catalog/sqlite3.h'),await readFile(process.env.PROSPEROMENU_SQLITE_HEADER || path.join(root,'../etahen-13.60/Source Code/include/sqlite3.h')));
