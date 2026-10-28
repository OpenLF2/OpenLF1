// SPDX-License-Identifier: MIT
// Copyright (c) 2026 OpenLF1

const gameStore = (() => {
  const databaseName = 'openlf1-game';
  const storeName = 'files';
  const key = 'game.zip';

  function open() {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(databaseName, 1);
      request.onupgradeneeded = () => request.result.createObjectStore(storeName);
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  }

  async function transact(mode, operation) {
    const database = await open();
    try {
      return await new Promise((resolve, reject) => {
        const transaction = database.transaction(storeName, mode);
        const request = operation(transaction.objectStore(storeName));
        transaction.oncomplete = () => resolve(request.result);
        request.onerror = () => reject(request.error);
        transaction.onerror = () => reject(transaction.error);
        transaction.onabort = () => reject(transaction.error);
      });
    } finally {
      database.close();
    }
  }

  return {
    load: () => transact('readonly', store => store.get(key)),
    save: file => transact('readwrite', store => store.put(file, key)),
    remove: () => transact('readwrite', store => store.delete(key))
  };
})();

const gameDownload = async onProgress => {
  const size = 465592;
  const sha256 = 'a37716a020b5445d5d12cc2494e0ee2c7c0a9f5de0395e7e1e2a0d52a0d80246';
  const sources = [
    'https://archive.org/download/msdos_Little_Fighter_1995/Little_Fighter_1995.zip',
    'https://archive.org/cors/msdos_Little_Fighter_1995/Little_Fighter_1995.zip'
  ];
  let failure;
  for (const source of sources) {
    try {
      const response = await fetch(source);
      if (!response.ok) throw new Error('HTTP ' + response.status);
      if (!response.body) throw new Error('Download stream unavailable');
      const chunks = [];
      let received = 0;
      const reader = response.body.getReader();
      onProgress(0, size);
      for (;;) {
        const {done, value} = await reader.read();
        if (done) break;
        chunks.push(value);
        received += value.length;
        onProgress(received, size);
      }
      const blob = new Blob(chunks, {type: 'application/zip'});
      if (blob.size !== size) throw new Error('Downloaded ' + blob.size + ' bytes, expected ' + size);
      if (crypto.subtle) {
        const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', await blob.arrayBuffer()));
        if (Array.from(digest, byte => byte.toString(16).padStart(2, '0')).join('') !== sha256) throw new Error('Checksum mismatch');
      }
      return new File([blob], 'Little_Fighter_1995.zip', {type: 'application/zip'});
    } catch (error) {
      failure = error;
    }
  }
  throw failure;
};
