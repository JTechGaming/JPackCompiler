const { LanguageClient, TransportKind } = require('vscode-languageclient/node');
const path = require('path');
const fs = require('fs');

let client;

function activate(context) {
    console.log('JPack extension activating...');

    const serverPath = path.join(context.extensionPath, 'server', 'JPackLanguageServer.exe');
    console.log('Server path:', serverPath);
    console.log('Server exists:', fs.existsSync(serverPath));

    if (!fs.existsSync(serverPath)) {
        console.error('JPackLanguageServer.exe not found!');
        return;
    }

    const serverOptions = {
        run: { command: serverPath, transport: TransportKind.stdio },
        debug: { command: serverPath, transport: TransportKind.stdio }
    };

    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'jpack' }],
        outputChannelName: 'JPack Language Server'
    };

    try {
        client = new LanguageClient('jpack', 'JPack Language Server', serverOptions, clientOptions);
        client.start();
        console.log('JPack language client started');
    } catch (e) {
        console.error('Failed to start JPack language client:', e);
    }
}

function deactivate() {
    if (client) return client.stop();
}

module.exports = { activate, deactivate };