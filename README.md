# vanity-ng

**vanity-ng** is OpenCL-powered vanity address generator for Bitcoin, Ethereum and Solana.

**vanity-ng** fully supports suffix searching and is 30% faster than [vanitygen++][vanitygen-pp] and 300% faster than [SolVanityCL][solvanitycl].

## installation

Windows: grab the [latest release][release]

Linux: get OpenCL headers, `git clone` and `make`

## usage examples

Ethereum `0x00000000...`: `vanity-ng -m eth -p 0x00000000`

Bitcoin `1...example`: `vanity-ng -m btc_base58 -s example`

## coming soon in r2...

* Ethereum ERC-55 and CREATE2 search
* case-insensitive Bitcoin/Solana search
* Bitcoin fork support (Litecoin etc)

## plz donate :>

Litecoin: `ltc1qyxqrj9p386xufevphlg7a6pnt0rrgwlzwz3smf`

[vanitygen-pp]: https://github.com/10gic/vanitygen-plusplus
[solvanitycl]: https://github.com/WincerChan/SolVanityCL
[release]: https://github.com/zhilemann/vanity-ng/releases/latest
