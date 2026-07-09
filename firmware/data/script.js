let cardData = [];
const scanLogTableBody = document.getElementById('scanLogTable').getElementsByTagName('tbody')[0];
const userTableBody = document.getElementById('userTable').getElementsByTagName('tbody')[0];
const importExportArea = document.getElementById('importExportArea');

let originalSsid = "";
let originalHidden = false;
let originalPwd = "";

let originalDisplayType = 1;
let originalFlipOled = false;

let currentUserCount = 0;

// Scan log display state
let hideData = false;      // HIDE DATA badge toggles blur on data columns
let cardDataMode = 'bin';  // 'hex' or 'bin' — DATA badge toggles this
let logLimit = 10;
let logExpanded = false;
let usersCache = [];       // mirrors users list for client-side name lookup
let learnerViewMode = false;
let wiegandFormats = [];
let showBits = true;
let showParity = true;
let parityCheckEnabled = false;
let lastParityCheckSetting = null;
let isPaused = false;

// FIX 1: Track if the user is currently editing the form
let unsavedChanges = false;

// NEW globals for dirty checking
let originalTimeout = 5000;
let originalCustomMessage = "";
let originalLedValid = 1;
let originalTamper = false;
let originalDisableEncoder = false;

// Mode toggle state — prevents spam clicking out of sync with hardware
let modePending = false;
let currentMode = 'raw'; // mirrors last confirmed hardware mode

// Virtual Screen Vars
let screenInterval = null;
const canvas = document.getElementById('oledCanvas');
const ctx = canvas?.getContext('2d');

// --- script.js ---

function checkDirty() {
    // 1. Gather Current Values
    const currSsid = document.getElementById('ap_ssid').value;
    const currPwd = document.getElementById('ap_pwd').value;
    const currHidden = document.getElementById('ssid_hidden').checked;

    const currTimeout = document.getElementById('timeoutSelect').value;
    const currMsg = document.getElementById('customMessage').value;
    const currLed = document.getElementById('ledValid').value;
    const currDisplay = document.getElementById('activeDisplayType').value;
    const currFlip = document.getElementById('flipOled').checked;
    const currTamper = document.getElementById('enable_tamper_detect').checked;
    const currDisableEncoder = document.getElementById('disable_encoder').checked; // NEW
    // 2. Compare
    let isDirty = false;

    if (currSsid !== originalSsid) isDirty = true;
    if (currPwd !== originalPwd) isDirty = true;
    if (currHidden !== originalHidden) isDirty = true;

    if (currTimeout != originalTimeout) isDirty = true;
    if (currMsg !== originalCustomMessage) isDirty = true;
    if (currLed != originalLedValid) isDirty = true;
    if (currDisplay != originalDisplayType) isDirty = true;
    if (currFlip !== originalFlipOled) isDirty = true;
    if (currTamper !== originalTamper) isDirty = true;
    if (currDisableEncoder !== originalDisableEncoder) isDirty = true;

    // 3. Update UI
    unsavedChanges = isDirty;
    const saveBtn = document.querySelector('#settings button');

    if (saveBtn) {
        // Ensure the button text is clean (no accidental duplication)
        if (saveBtn.childNodes.length === 0 || (saveBtn.firstChild.nodeType === 3 && saveBtn.firstChild.nodeValue !== "Save Settings")) {
            saveBtn.firstChild.nodeValue = "Save Settings";
        }

        let badge = document.getElementById('unsavedBadge');

        // Create badge if it doesn't exist
        if (!badge) {
            badge = document.createElement('span');
            badge.id = 'unsavedBadge';
            // We do NOT use the generic .badge class here to avoid style conflicts.
            // The ID selector in CSS handles all the styling.
            badge.className = 'collapsed';
            badge.textContent = 'UNSAVED';
            saveBtn.appendChild(badge);
        }

        // Toggle Visibility
        if (unsavedChanges) {
            badge.classList.remove('collapsed');
        } else {
            badge.classList.add('collapsed');
        }
    }
}

function updateScreen() {
    if (document.getElementById('monitor').classList.contains('hidden')) return;

    // 1. Check if the element even exists in Firefox's view
    const canvas = document.getElementById('oledCanvas');
    if (!canvas) {
        console.log("Firefox Error: oledCanvas not found.");
        return;
    }

    let hwRotation = 0; // will be populated from response header

    fetch('/screen?t=' + Date.now())
        .then(response => {
            // Read the hardware rotation the firmware reported.
            // Header is "X-Oled-Rotation: 0|1|2|3"
            const rotHeader = response.headers.get('X-Oled-Rotation');
            hwRotation = rotHeader !== null ? parseInt(rotHeader, 10) : 0;
            return response.arrayBuffer();
        })
        .then(buffer => {
            const data = new Uint8Array(buffer);

            // Determine native buffer dimensions (128x64 / 1024 bytes)
            const bufW = 128;
            const bufH = 64;
            const pages = 8;

            // Decode raw SSD1306 GDDRAM bytes into an ImageData at native size
            const offscreen = document.createElement('canvas');
            offscreen.width = bufW;
            offscreen.height = bufH;
            const offCtx = offscreen.getContext('2d');
            const imageData = offCtx.createImageData(bufW, bufH);
            const r = 0, g = 209, b = 255;

            for (let page = 0; page < pages; page++) {
                for (let x = 0; x < bufW; x++) {
                    const byte = data[page * bufW + x];
                    for (let bit = 0; bit < 8; bit++) {
                        const y = page * 8 + bit;
                        const index = (y * bufW + x) * 4;
                        const pixelOn = (byte >> bit) & 1;
                        imageData.data[index] = pixelOn ? r : 0;
                        imageData.data[index + 1] = pixelOn ? g : 0;
                        imageData.data[index + 2] = pixelOn ? b : 0;
                        imageData.data[index + 3] = 255;
                    }
                }
            }
            offCtx.putImageData(imageData, 0, 0);

            // Apply un-rotation transform so the virtual screen is always upright.
            //
            // SSD1306 GDDRAM layout vs. hardware rotation:
            //   rot 0  → buffer is upright in the browser → no transform needed.
            //   rot 2  → buffer is stored upside-down → rotate 180° to correct.
            //   rot 1  → 90° CW hardware (future) → rotate 90° CCW to correct.
            //   rot 3  → 90° CCW hardware (future) → rotate 90° CW to correct.
            //
            // For rotations 0 & 2 the canvas stays bufW × bufH.
            // For rotations 1 & 3 (future) width and height swap.
            let displayW = bufW;
            let displayH = bufH;
            if (hwRotation === 1 || hwRotation === 3) {
                displayW = bufH;
                displayH = bufW;
            }

            const ctx = canvas.getContext('2d');
            canvas.width = displayW;
            canvas.height = displayH;
            ctx.save();

            switch (hwRotation) {
                case 0:
                    // Buffer is upright — no transform needed
                    break;
                case 2:
                    // Buffer stored upside-down → rotate 180° around canvas centre
                    ctx.translate(displayW, displayH);
                    ctx.rotate(Math.PI);
                    break;
                case 1:
                    // 90° CW hardware → rotate 90° CCW to correct (future)
                    ctx.translate(0, displayH);
                    ctx.rotate(-Math.PI / 2);
                    break;
                case 3:
                    // 90° CCW hardware → rotate 90° CW to correct (future)
                    ctx.translate(displayW, 0);
                    ctx.rotate(Math.PI / 2);
                    break;
                default:
                    break;
            }

            ctx.drawImage(offscreen, 0, 0);
            ctx.restore();
        })
        .catch(err => console.error("Firefox Fetch Error:", err));
}

function toggleHideData() {
    hideData = !hideData;
    const badge = document.getElementById('badgeHideData');
    if (badge) badge.className = 'badge badge-clickable ' + (hideData ? 'badge-purple' : 'badge-gray');
    // Only blur tbody data cells — not the header
    document.querySelectorAll('#scanLogTable tbody td.col-data').forEach(el => {
        el.classList.toggle('data-blurred', hideData);
    });
}

function toggleLearnerView() {
    learnerViewMode = !learnerViewMode;
    const badge = document.getElementById('badgeLearnerView');
    if (badge) {
        badge.className = 'badge badge-clickable ' + (learnerViewMode ? 'badge-purple' : 'badge-gray');
    }
    const table = document.getElementById('scanLogTable');
    if (table) {
        table.classList.toggle('learner-view-active', learnerViewMode);
    }
}

function toggleShowBits() {
    showBits = !showBits;
    const badge = document.getElementById('badgeToggleBits');
    if (badge) {
        badge.className = 'badge badge-clickable ' + (showBits ? 'badge-purple' : 'badge-gray');
    }
    const table = document.getElementById('scanLogTable');
    if (table) {
        table.classList.toggle('hide-bits-badge', !showBits);
    }
}

function toggleShowParity() {
    showParity = !showParity;
    const badge = document.getElementById('badgeToggleParity');
    if (badge) {
        badge.className = 'badge badge-clickable ' + (showParity ? 'badge-purple' : 'badge-gray');
    }
    const table = document.getElementById('scanLogTable');
    if (table) {
        table.classList.toggle('hide-parity-badge', !showParity);
    }
}

function syncParityToggleBadge() {
    const parityToggle = document.getElementById('badgeToggleParity');
    const table = document.getElementById('scanLogTable');
    
    if (parityToggle) {
        if (parityCheckEnabled) {
            parityToggle.style.display = 'inline-block';
            if (lastParityCheckSetting === false || lastParityCheckSetting === null) {
                showParity = true;
                parityToggle.className = 'badge badge-purple badge-clickable';
                if (table) {
                    table.classList.remove('hide-parity-badge');
                }
            }
        } else {
            parityToggle.style.display = 'none';
            showParity = false;
            parityToggle.className = 'badge badge-gray badge-clickable';
            if (table) {
                table.classList.add('hide-parity-badge');
            }
        }
    }
    lastParityCheckSetting = parityCheckEnabled;
}

function toggleCardDataMode() {
    cardDataMode = (cardDataMode === 'hex') ? 'bin' : 'hex';
    const badge = document.getElementById('badgeCardData');
    if (badge) badge.textContent = cardDataMode === 'hex' ? 'DATA: HEX' : 'DATA: BIN';
    
    const learnerBadge = document.getElementById('badgeLearnerView');
    const table = document.getElementById('scanLogTable');

    if (cardDataMode === 'hex') {
        learnerViewMode = false;
        if (learnerBadge) {
            learnerBadge.classList.add('hide-learner-badge');
            learnerBadge.className = 'badge badge-gray badge-clickable hide-learner-badge';
        }
    } else {
        if (learnerBadge) {
            learnerBadge.classList.remove('hide-learner-badge');
            learnerBadge.className = 'badge badge-gray badge-clickable';
        }
    }

    if (table) {
        table.classList.toggle('data-mode-hex', cardDataMode === 'hex');
        table.classList.toggle('learner-view-active', learnerViewMode);
    }
}

function toggleLogExpand() {
    logExpanded = !logExpanded;
    logLimit = logExpanded ? 25 : 10;
    const btn = document.getElementById('logExpandBtn');
    if (btn) btn.innerHTML = logExpanded ? '&#9650; Show 10' : '&#9660; Show 25';
    updateScanLog();
}

function updateScanLog() {
    fetch('/getCards?t=' + Date.now())
        .then(r => r.json())
        .then(data => {
            const table = document.getElementById('scanLogTable');
            if (table) {
                table.classList.toggle('data-mode-hex', cardDataMode === 'hex');
            }
            scanLogTableBody.innerHTML = '';
            const slice = data.slice(-logLimit).reverse(); // newest first

            slice.forEach((card, index) => {
                const row = scanLogTableBody.insertRow();

                // Row color always reflects authorization state
                if (card.status === 'Authorized') row.classList.add('authorized');
                if (card.status === 'Unauthorized') row.classList.add('unauthorized');

                // Col 0: #
                row.insertCell(0).textContent = index + 1;
                // Col 1: Name — client-side lookup by FC+CN regardless of mode
                const cellName = row.insertCell(1);
                const hasFormat = card.hasFormat !== false;
                const fc = hasFormat ? (card.facilityCode ?? '') : '';
                const cn = hasFormat ? (card.cardNumber ?? '') : '';
                const matchedUser = hasFormat ? usersCache.find(
                    u => String(u.facilityCode) === String(fc) && String(u.cardNumber) === String(cn)
                ) : null;
                const parityFailed = (card.parityStatus === 0);
                if (matchedUser && !parityFailed) {
                    cellName.textContent = matchedUser.name;
                } else if (hasFormat && (fc !== '' || cn !== '')) {
                    if (parityFailed) {
                        cellName.textContent = '—';
                    } else {
                        cellName.innerHTML = `<span class="badge badge-add-user badge-clickable badge-scan" onclick="quickAddUser('${fc}', '${cn}')">+ ADD</span>`;
                    }
                } else {
                    cellName.textContent = '—';
                }

                // Col 2: Decode — FC: x CN: y (col-data)
                const cellDecode = row.insertCell(2);
                cellDecode.className = 'col-data';
                if (hideData) cellDecode.classList.add('data-blurred');
                const ps = card.parityStatus;
                const format = hasFormat ? wiegandFormats.find(f => f.bitCount === card.bitCount) : null;
                const fcBadgeClass = format ? 'badge-fc' : 'badge-gray';
                const cnBadgeClass = format ? 'badge-cn' : 'badge-gray';
                const fcDisp = hasFormat ? fc : '--';
                const cnDisp = hasFormat ? cn : '--';
                cellDecode.innerHTML = `<span style="color: var(--muted); margin-right: 4px;">FC:</span><span class="badge ${fcBadgeClass} badge-scan" style="margin-right: 16px;">${fcDisp}</span><span style="color: var(--muted); margin-right: 4px;">CN:</span><span class="badge ${cnBadgeClass} badge-scan">${cnDisp}</span>`;
                // Col 3: Card Data — hex or binary + <num>b badge always + parity badge (if enabled) + PAD badge (col-data)
                const cellCardData = row.insertCell(3);
                cellCardData.className = 'col-data';
                if (hideData) cellCardData.classList.add('data-blurred');
                
                let cellHTML = '';
                
                // 1. Construct BIN view elements
                const binStr = card.rawCardData || '';
                let binDisplayedText = binStr;
                if (binStr && format) {
                    const fcStart = format.facilityCodeStart - 1;
                    const fcEnd = format.facilityCodeEnd;
                    const cnStart = format.cardNumberStart - 1;
                    const cnEnd = format.cardNumberEnd;
                    
                    let ranges = [
                        { type: 'fc', start: fcStart, end: fcEnd },
                        { type: 'cn', start: cnStart, end: cnEnd }
                    ].sort((a, b) => a.start - b.start);
                    
                    let highlightedBin = "";
                    let lastIdx = 0;
                    for (let r of ranges) {
                        if (r.start > lastIdx) {
                            highlightedBin += binStr.substring(lastIdx, r.start);
                        }
                        const className = r.type === 'fc' ? 'fc-highlight' : 'cn-highlight';
                        if (r.start >= 0 && r.end <= binStr.length && r.start < r.end) {
                            highlightedBin += `<span class="${className}">${binStr.substring(r.start, r.end)}</span>`;
                        } else {
                            highlightedBin += binStr.substring(Math.max(0, r.start), Math.min(binStr.length, r.end));
                        }
                        lastIdx = r.end;
                    }
                    if (lastIdx < binStr.length) {
                        highlightedBin += binStr.substring(lastIdx);
                    }
                    binDisplayedText = highlightedBin;
                }
                
                // Prefix container with absolute-positioned layers
                cellHTML += `<span class="data-prefix">`;
                cellHTML += `<span class="bin-prefix">BIN:</span>`;
                cellHTML += `<span class="hex-prefix">HEX:</span>`;
                cellHTML += `</span>`;
                
                const binLen = binStr ? binStr.length : 0;
                const hexStr = card.hexData || '';
                let hexLen = hexStr ? hexStr.length : 0;
                if (hexStr && card.padCount && card.padCount > 0) {
                    hexLen += 6 + String(card.padCount).length;
                }
                
                // Single badge container with sliding wrappers inside
                cellHTML += `<span class="badge badge-gray badge-scan badge-clickable card-data-badge" style="--bin-width: ${binLen}ch; --hex-width: ${hexLen}ch; margin-right: 4px;">`;
                if (binStr) {
                    cellHTML += `<span class="bin-data-wrapper"><a href="#" onclick="copyToClipboard('${binStr}');return false;" class="card-data-link">${binDisplayedText}</a></span>`;
                }
                if (hexStr) {
                    let hexInner = `<a href="#" onclick="copyToClipboard('${hexStr}');return false;" class="card-data-link">${hexStr}</a>`;
                    if (card.padCount && card.padCount > 0) {
                        hexInner += ` <span style="color: var(--text); margin-left: 4px;">PAD: ${card.padCount}</span>`;
                    }
                    cellHTML += `<span class="hex-data-wrapper">${hexInner}</span>`;
                }
                cellHTML += `</span>`;
                
                if (card.bitCount) {
                    cellHTML += `<span class="badge badge-gray badge-scan badge-bitcount" style="margin-right: 8px; text-transform: none;">${card.bitCount}b</span>`;
                }
                if (parityCheckEnabled && ps !== -1 && ps !== undefined) {
                    let parityText = '';
                    if (ps === 1) parityText = 'PASS';
                    else if (ps === 0) parityText = 'FAIL';
                    else if (ps === 2) parityText = 'N/A';
                    
                    if (parityText) {
                        cellHTML += `<span class="badge badge-gray badge-scan badge-parity" style="margin-right: 8px;">${parityText}</span>`;
                    }
                }
                cellCardData.innerHTML = cellHTML;
            });

            // Pad empty rows to fill logLimit
            for (let i = slice.length; i < logLimit; i++) {
                const row = scanLogTableBody.insertRow();
                for (let c = 0; c < 4; c++) row.insertCell(c);
                row.cells[0].textContent = i + 1;
                row.cells[2].className = 'col-data';
                row.cells[3].className = 'col-data';
                if (hideData) {
                    row.cells[2].classList.add('data-blurred');
                    row.cells[3].classList.add('data-blurred');
                }
            }
        })
        .catch(err => console.error('Error fetching scan log:', err));
}

function clearLog() {
    if (!confirm('Clear all scan history?')) return;
    fetch('/clearCards', { method: 'POST' })
        .then(response => {
            if (response.ok) {
                updateScanLog();
            } else {
                alert('Failed to clear log.');
            }
        })
        .catch(err => console.error('Error clearing log:', err));
}

function clearUsers() {
    if (!confirm('Clear all users from the device?')) return;
    fetch('/clearUsers', { method: 'POST' })
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('All users cleared successfully.');
            } else {
                alert('Failed to clear users.');
            }
        })
        .catch(err => console.error('Error clearing users:', err));
}

function updateUserTable() {
    return fetch('/getUsers?t=' + Date.now())
        .then(response => response.json())
        .then(data => {
            currentUserCount = data.length;
            usersCache = data; // keep a local copy for name lookup in the scan log
            userTableBody.innerHTML = '';
            data.forEach((user, index) => {
                let row = userTableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellFacilityCode = row.insertCell(1);
                let cellCardNumber = row.insertCell(2);
                let cellName = row.insertCell(3);
                let cellFlag = row.insertCell(4);
                let cellAction = row.insertCell(5);

                cellIndex.innerHTML = index + 1;
                cellFacilityCode.innerHTML = user.facilityCode;
                cellCardNumber.innerHTML = user.cardNumber;
                cellName.innerHTML = user.name;
                cellFlag.innerHTML = user.flag || '';
                cellAction.innerHTML = '<button onclick="editUser(' + index + ')">Edit</button> <button onclick="deleteUser(' + index + ')">Delete</button>';
            });

            let inputRow = userTableBody.insertRow();
            inputRow.className = 'inputRow';
            let cellIndex = inputRow.insertCell(0);
            let cellFacilityCode = inputRow.insertCell(1);
            let cellCardNumber = inputRow.insertCell(2);
            let cellName = inputRow.insertCell(3);
            let cellFlag = inputRow.insertCell(4);
            let cellAction = inputRow.insertCell(5);

            cellFacilityCode.innerHTML = '<input type="text" id="newFacilityCode" inputmode="numeric" maxlength="9" oninput="this.value = this.value.replace(/[^0-9]/g, \'\'); checkUserInputsDirty();" onkeydown="if(event.key === \'Enter\') document.getElementById(\'saveUserButton\').click()">';
            cellCardNumber.innerHTML = '<input type="text" id="newCardNumber" inputmode="numeric" maxlength="9" oninput="this.value = this.value.replace(/[^0-9]/g, \'\'); checkUserInputsDirty();" onkeydown="if(event.key === \'Enter\') document.getElementById(\'saveUserButton\').click()">';
            cellName.innerHTML = '<input type="text" id="newName" maxlength="12" oninput="checkUserInputsDirty();" onkeydown="if(event.key === \'Enter\') document.getElementById(\'saveUserButton\').click()">';
            cellFlag.innerHTML = '<input type="text" id="newFlag" maxlength="21" oninput="checkUserInputsDirty();" onkeydown="if(event.key === \'Enter\') document.getElementById(\'saveUserButton\').click()">';
            cellAction.innerHTML = '<button id="saveUserButton" onclick="addUser()">Save</button> <button id="clearUserFieldsButton" onclick="clearUserFields()">Clear</button> <button id="cancelEditButton" style="display:none;" onclick="cancelEdit()">Cancel</button>';
            checkUserInputsDirty();
        })
        .catch(error => console.error('Error fetching user data:', error));
}

function checkUserInputsDirty() {
    const fcVal = document.getElementById('newFacilityCode')?.value || '';
    const cnVal = document.getElementById('newCardNumber')?.value || '';
    const nameVal = document.getElementById('newName')?.value || '';
    const flagVal = document.getElementById('newFlag')?.value || '';

    const isDirty = (fcVal !== '' || cnVal !== '' || nameVal !== '' || flagVal !== '');

    const saveBtn = document.getElementById('saveUserButton');
    const inputRow = document.querySelector('.inputRow');

    if (saveBtn) {
        if (isDirty) {
            saveBtn.classList.add('pulse-gold');
        } else {
            saveBtn.classList.remove('pulse-gold');
        }
    }

    if (inputRow) {
        if (isDirty) {
            inputRow.classList.add('row-active-gold');
        } else {
            inputRow.classList.remove('row-active-gold');
        }
    }
}

function quickAddUser(fc, cn) {
    cancelEdit();

    const content = document.querySelector(".contentCollapsible");
    if (content && content.style.display !== "block") {
        content.style.display = "block";
    }

    const fcInput = document.getElementById('newFacilityCode');
    const cnInput = document.getElementById('newCardNumber');
    const nameInput = document.getElementById('newName');

    if (fcInput) fcInput.value = fc;
    if (cnInput) cnInput.value = cn;
    if (nameInput) {
        nameInput.value = '';
        nameInput.focus();
    }
    checkUserInputsDirty();
}

function addUser() {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;
    const flag = document.getElementById('newFlag').value;

    if (!validateUserInput(facilityCode, cardNumber, name, flag, -1)) return;

    fetch(`/addUser?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${encodeURIComponent(name)}&flag=${encodeURIComponent(flag)}&t=${Date.now()}`)
        .then(response => {
            if (response.ok) {
                updateUserTable().then(() => {
                    updateScanLog();
                });
                document.getElementById('newFacilityCode').value = '';
                document.getElementById('newCardNumber').value = '';
                document.getElementById('newName').value = '';
                document.getElementById('newFlag').value = '';
            } else {
                response.text().then(text => alert('Failed: ' + text));
            }
        })
        .catch(error => console.error('Error adding card:', error));
}

function deleteUser(index) {
    fetch(`/deleteUser?index=${index}&t=${Date.now()}`)
        .then(response => {
            if (response.ok) {
                updateUserTable().then(() => {
                    updateScanLog();
                });
                alert('User deleted successfully');
            } else {
                alert('Failed to delete user');
            }
        })
        .catch(error => console.error('Error deleting user:', error));
}

function showSection(section) {
    document.getElementById('monitor').classList.add('hidden');
    document.getElementById('settings').classList.add('hidden');

    document.getElementById(section).classList.remove('hidden');

    // Manage the Polling Interval
    if (section === 'monitor') {
        if (!screenInterval) screenInterval = setInterval(updateScreen, 150); // ~6 FPS
    } else {
        if (screenInterval) {
            clearInterval(screenInterval);
            screenInterval = null;
        }
    }
}

function showSectionWithRefresh(section) {
    showSection(section);
    if (section === 'monitor') {
        fetchSettings();
    }
}

function toggleCollapsible() {
    const content = document.querySelector(".contentCollapsible");
    content.style.display = content.style.display === "block" ? "none" : "block";
}

function toggleFlipOption() {
    const displayType = document.getElementById('activeDisplayType').value;
    const container = document.getElementById('flipOledContainer');
    const flipCheckbox = document.getElementById('flipOled');

    if (displayType === "1") {
        // LCD Mode selected
        container.style.display = 'none';

        // Logic: If user clicks LCD, force Flip to FALSE
        if (flipCheckbox.checked) {
            flipCheckbox.checked = false;
            // Run dirty check so the "UNSAVED" badge appears immediately
            checkDirty(); // TODO: Combine checkDirty and checkChanges because they really do the same thing
            checkChanges();
        }
    } else {
        // OLED Mode selected
        container.style.display = 'inline-block';
    }
}

function updateUserIndicator(settings) {
    const mode = (settings.device_mode || settings.mode || '').toString().toLowerCase();
    const isUserOn = (mode === 'user');
    const statusEl = document.getElementById('userStatus');
    const hintEl = document.getElementById('userHint');
    if (statusEl) {
        statusEl.textContent = isUserOn ? 'ON' : 'OFF';
        statusEl.classList.toggle('user-on', isUserOn);
        statusEl.classList.toggle('user-off', !isUserOn);
    }
    if (hintEl) hintEl.style.display = isUserOn ? 'none' : 'block';
}

function updateTamperIndicator(settings) {
    const container = document.getElementById('tamperIndicator');
    const statusEl = document.getElementById('tamperStatus');
    if (!container || !statusEl) return;

    const isEnabled = settings.enable_tamper_detect ? true : false;
    const isTripped = settings.tamper_tripped ? true : false;

    statusEl.classList.remove('status-green', 'status-red', 'status-yellow');

    if (!isEnabled) {
        container.style.display = 'none';
    } else {
        container.style.display = 'block';
        if (isTripped) {
            statusEl.textContent = 'TAMPERING DETECTED!';
            statusEl.classList.add('status-yellow');
        } else {
            statusEl.textContent = 'ENABLED';
            statusEl.classList.add('status-green');
        }
    }
}

function saveSettings() {
    const pwdInput = document.getElementById('ap_pwd').value;
    const ssidInput = document.getElementById('ap_ssid').value;
    const hiddenInput = document.getElementById('ssid_hidden').checked;
    const disableEncoder = document.getElementById('disable_encoder').checked;

    if (!ssidInput || ssidInput.trim().length === 0) {
        alert("Cannot Save: SSID cannot be empty.");
        document.getElementById('ap_ssid').focus();
        return;
    }

    if (pwdInput.includes(" ")) {
        alert("Cannot Save: Password cannot contain spaces.");
        document.getElementById('ap_pwd').focus();
        return;
    }

    if (pwdInput.length > 0 && pwdInput.length < 8) {
        alert("Cannot Save: Password must be empty (Open Network) or at least 8 characters.");
        document.getElementById('ap_pwd').focus();
        return;
    }

    const pwdChanged = (pwdInput !== originalPwd);
    const ssidChanged = (ssidInput !== originalSsid);
    const hiddenChanged = (hiddenInput !== originalHidden);
    const wifiChanged = (pwdChanged || ssidChanged || hiddenChanged);

    const currentDisplay = parseInt(document.getElementById('activeDisplayType').value, 10);
    const currentFlip = document.getElementById('flipOled').checked;

    const displayChanged = (currentDisplay !== originalDisplayType) || (currentFlip !== originalFlipOled);
    const rebootRequired = wifiChanged || displayChanged;

    if (wifiChanged) {
        if (pwdChanged) {
            const userConfirmed = prompt("Wifi password has been changed. Please re-type the new password to confirm:");
            if (userConfirmed === null) return;
            if (userConfirmed !== pwdInput) {
                alert("Passwords do not match. Settings NOT saved.");
                return;
            }
        }
        else if (!confirm("WiFi settings have changed. The device will reboot. Continue?")) {
            return;
        }
    }

    const timeout = document.getElementById('timeoutSelect').value;
    const customMessage = document.getElementById('customMessage').value;
    const ledValid = document.getElementById('ledValid').value;
    const activeDisplayType = document.getElementById('activeDisplayType').value;
    const enableTamperDetect = document.getElementById('enable_tamper_detect').checked;

    let settings = {
        display_timeout: parseInt(timeout, 10),
        ap_ssid: ssidInput,
        ap_pwd: pwdInput,
        ssid_hidden: hiddenInput ? 1 : 0,
        custom_message: customMessage,
        led_valid: parseInt(ledValid, 10),
        active_display_type: parseInt(activeDisplayType, 10),
        flip_oled_display: currentFlip,
        enable_tamper_detect: enableTamperDetect,
        should_reboot: rebootRequired,
        disable_encoder: disableEncoder
    };

    fetch('/saveSettings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    })
        .then(response => {
            if (response.ok) {
                if (rebootRequired) {
                    alert('Settings saved! Device is REBOOTING now. Please reconnect.');
                } else {
                    alert('Settings saved successfully.');
                    // RESET DIRTY FLAG ON SUCCESSFUL SAVE
                    unsavedChanges = false;
                    fetchSettings(true);
                }
                document.getElementById('rebootWarning').style.display = 'none';
            } else {
                alert('Failed to save settings');
            }
        })
        .catch(error => console.error('Error saving settings:', error));
}


function fetchSettings(forceUpdateUI = false) {
    // FIX 2: Cache busting timestamp
    fetch('/getSettings?t=' + Date.now())
        .then(response => response.json())
        .then(data => {
            updateUserIndicator(data);
            updateTamperIndicator(data);

            // FIX 1: Only update UI if there are NO unsaved changes 
            // OR if we forced it (like immediately after a save)
            if (unsavedChanges && !forceUpdateUI) {
                console.log("Skipping UI update: Unsaved changes present.");
                return;
            }

            updateSettingsUI(data);
        })
        .catch(error => console.error('Error fetching settings:', error));
}

// --- script.js ---

function updateSettingsUI(settings) {
    const mode = settings.device_mode || settings.mode || '';

    // FIX: Falsey 0 logic included here
    const displayTimeout = (settings.display_timeout !== undefined)
        ? settings.display_timeout
        : (settings.displayTimeout !== undefined ? settings.displayTimeout : '');

    const ledValid = (settings.led_valid !== undefined)
        ? settings.led_valid
        : (settings.ledValid !== undefined ? settings.ledValid : 1);

    const apSsid = settings.ap_ssid || settings.apSsid || '';
    const apPass = settings.ap_pwd || settings.apPassphrase || '';
    const ssidHidden = (settings.ssid_hidden !== undefined) ? settings.ssid_hidden : settings.ssidHidden;
    const customMessage = settings.custom_message || settings.customMessage || '';
    const activeDisplayType = settings.active_display_type || settings.activeDisplayType || '';
    const enableTamperDetect = (settings.enable_tamper_detect !== undefined) ? settings.enable_tamper_detect : settings.enableTamperDetect;
    const version = settings.version || settings.version || '';
    const disableEncoder = (settings.disable_encoder !== undefined) ? settings.disable_encoder : false;
    parityCheckEnabled = (settings.enable_parity_check !== undefined) ? settings.enable_parity_check : false;

    // Get Pause State
    isPaused = (settings.is_paused === true);

    // 1. Populate UI Inputs
    if (document.getElementById('timeoutSelect')) document.getElementById('timeoutSelect').value = displayTimeout;
    if (document.getElementById('ap_ssid')) document.getElementById('ap_ssid').value = apSsid;
    if (document.getElementById('ap_pwd')) document.getElementById('ap_pwd').value = apPass;
    if (document.getElementById('ssid_hidden') && typeof ssidHidden !== 'undefined') document.getElementById('ssid_hidden').checked = ssidHidden;
    if (document.getElementById('customMessage')) document.getElementById('customMessage').value = customMessage;
    if (document.getElementById('ledValid')) document.getElementById('ledValid').value = ledValid;
    if (document.getElementById('activeDisplayType')) document.getElementById('activeDisplayType').value = activeDisplayType;
    if (document.getElementById('enable_tamper_detect')) document.getElementById('enable_tamper_detect').checked = enableTamperDetect;
    if (document.getElementById('versionValue')) document.getElementById('versionValue').textContent = version;
    if (document.getElementById('flipOled')) document.getElementById('flipOled').checked = settings.flip_oled_display;
    if (document.getElementById('disable_encoder')) document.getElementById('disable_encoder').checked = disableEncoder;

    // 2. [NEW] Update Mode Banner (Top Right)
    const modeBox = document.getElementById('modeBox');
    const modeValueEl = document.getElementById('modeValue');

    if (modeBox && modeValueEl) {
        modeBox.classList.remove('mode-paused', 'mode-raw', 'mode-user', 'mode-pending');
        if (isPaused) {
            modeValueEl.textContent = "PAUSED";
            modeBox.classList.add('mode-paused');
        } else {
            currentMode = (mode || 'raw').toString().toLowerCase();
            modeValueEl.textContent = (mode || '').toString().toUpperCase();
            if (currentMode === 'raw') {
                modeBox.classList.add('mode-raw');
            } else if (currentMode === 'user') {
                modeBox.classList.add('mode-user');
            }
        }
    }

    // 3. Update Pause Button State
    const navBtnPause = document.getElementById('navBtnPause');
    if (navBtnPause) {
        if (isPaused) {
            navBtnPause.classList.add('paused');
            navBtnPause.setAttribute('title', 'Un-pause DoorSim');
        } else {
            navBtnPause.classList.remove('paused');
            navBtnPause.setAttribute('title', 'Pause DoorSim');
        }
    }

    // 4. Update Parity Button State
    const navBtnParity = document.getElementById('navBtnParity');
    if (navBtnParity) {
        if (parityCheckEnabled) {
            navBtnParity.classList.add('parity-on');
        } else {
            navBtnParity.classList.remove('parity-on');
        }
    }

    // 5. STORE ORIGINAL VALUES
    originalSsid = apSsid;
    originalPwd = apPass;
    originalHidden = (ssidHidden == 1 || ssidHidden === true);
    originalDisplayType = activeDisplayType;
    originalFlipOled = settings.flip_oled_display;

    // New globals
    originalTimeout = displayTimeout;
    originalCustomMessage = customMessage;
    originalLedValid = ledValid;
    originalTamper = enableTamperDetect;
    originalDisableEncoder = disableEncoder;

    // Listeners
    const displaySelect = document.getElementById('activeDisplayType');
    if (displaySelect) {
        displaySelect.addEventListener('change', toggleFlipOption);
    }

    toggleFlipOption();
    checkDirty();
    syncParityToggleBadge();
}

function toggleMode() {
    if (modePending || isPaused) return; // Ignore clicks while waiting for hardware confirmation

    modePending = true;
    const newMode = (currentMode === 'raw') ? 'user' : 'raw';

    const modeBox = document.getElementById('modeBox');
    if (modeBox) {
        modeBox.classList.add('mode-pending');
        modeBox.classList.remove('mode-raw', 'mode-user', 'mode-paused');
        modeBox.classList.add('mode-' + newMode);
        const modeValueEl = document.getElementById('modeValue');
        if (modeValueEl) modeValueEl.textContent = newMode.toUpperCase();
    }

    fetch('/setMode', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: newMode })
    })
        .then(response => {
            if (!response.ok) throw new Error('Server rejected mode change');
            return response.json();
        })
        .then(data => {
            // Confirmed — fetch settings to fully sync UI
            fetchSettings(true);
        })
        .catch(err => {
            console.error('Mode toggle failed:', err);
            // Revert visual to previously confirmed mode
            if (modeBox) {
                modeBox.classList.remove('mode-pending', 'mode-raw', 'mode-user');
                modeBox.classList.add('mode-' + currentMode);
                const modeValueEl = document.getElementById('modeValue');
                if (modeValueEl) modeValueEl.textContent = currentMode.toUpperCase();
            }
        })
        .finally(() => {
            modePending = false;
        });
}

function toggleParitySetting() {
    fetch('/toggleParity', { method: 'POST' })
        .then(response => response.text())
        .then(state => {
            parityCheckEnabled = (state === 'ON');
            const navBtnParity = document.getElementById('navBtnParity');
            if (navBtnParity) {
                if (parityCheckEnabled) {
                    navBtnParity.classList.add('parity-on');
                } else {
                    navBtnParity.classList.remove('parity-on');
                }
            }
            syncParityToggleBadge();
        })
        .catch(err => console.error('Error toggling parity:', err));
}

function togglePause() {
    fetch('/togglePause', { method: 'POST' })
        .then(response => response.text())
        .then(status => {
            // Trigger a settings fetch to update the UI button state immediately
            fetchSettings(true);
        })
        .catch(err => alert("Error toggling pause state."));
}


function exportData() {
    const dataString = JSON.stringify(usersCache);
    importExportArea.value = dataString;
    importExportArea.select();
    document.execCommand('copy');
    alert('Data exported to clipboard');
}

function confirmExport() {
    if (confirm(`Download the current users.json configuration with ${currentUserCount} users?`)) {
        window.location.href = '/downloadUsers';
    }
}

function confirmSampleDownload() {
    const message = "Download sample file?\n\nMake sure to remove .sample extension and before re-uploading!";
    if (confirm(message)) {
        window.location.href = '/downloadSample';
    }
}

function uploadUserFile() {
    const fileInput = document.getElementById('usersUpload');
    const statusDiv = document.getElementById('uploadStatus');
    const btn = document.getElementById('btnImport');

    if (fileInput.files.length === 0) {
        alert("Please select a users.json file first.");
        return;
    }

    const file = fileInput.files[0];
    const reader = new FileReader();

    reader.onload = function (e) {
        try {
            const json = JSON.parse(e.target.result);
            let count = 0;
            if (json.users && Array.isArray(json.users)) {
                count = json.users.length;
            } else {
                alert("Invalid file format: 'users' array is missing.");
                return;
            }

            // Check for duplicate users in uploaded file
            const keys = new Set();
            for (let i = 0; i < json.users.length; i++) {
                const user = json.users[i];
                if (user.facilityCode === undefined || user.cardNumber === undefined) {
                    alert(`Invalid file format: User at index ${i} is missing facilityCode or cardNumber.`);
                    return;
                }
                const fcStr = String(user.facilityCode);
                const cnStr = String(user.cardNumber);
                if (!/^\d{1,9}$/.test(fcStr)) {
                    alert(`Invalid file format: User at index ${i} has an invalid facilityCode ("${fcStr}"). Must be 1-9 digits.`);
                    return;
                }
                if (!/^\d{1,9}$/.test(cnStr)) {
                    alert(`Invalid file format: User at index ${i} has an invalid cardNumber ("${cnStr}"). Must be 1-9 digits.`);
                    return;
                }
                const key = `${user.facilityCode}-${user.cardNumber}`;
                if (keys.has(key)) {
                    alert(`Invalid file format: Duplicate user found in file (FC: ${user.facilityCode}, CN: ${user.cardNumber}).`);
                    return;
                }
                keys.add(key);

                const userName = user.name !== undefined && user.name !== null ? String(user.name) : "";
                const userFlag = user.flag !== undefined && user.flag !== null ? String(user.flag) : "";
                if (userName.length > 12) {
                    alert(`Invalid file format: User at index ${i} has a name exceeding 12 characters ("${userName}").`);
                    return;
                }
                if (userFlag.length > 21) {
                    alert(`Invalid file format: User at index ${i} has a flag exceeding 21 characters ("${userFlag}").`);
                    return;
                }
            }

            const message = `Import ${count} users?\n\nWARNING: This will overwrite all current users!`;
            if (!confirm(message)) return;

            const formData = new FormData();
            formData.append("file", file);

            btn.disabled = true;
            btn.textContent = "Verifying...";
            statusDiv.textContent = "";

            fetch('/uploadUsers', {
                method: 'POST',
                body: formData
            })
                .then(async response => {
                    const text = await response.text();
                    if (response.ok) {
                        statusDiv.style.color = "#236b2b";
                        statusDiv.textContent = "Success!";
                        alert(text);
                        fileInput.value = "";
                        updateUserTable();
                    } else {
                        throw new Error(text);
                    }
                })
                .catch(error => {
                    console.error('Upload Error:', error);
                    statusDiv.style.color = "#8b1f1f";
                    statusDiv.textContent = "Failed";
                    alert("Import Failed:\n" + error.message);
                })
                .finally(() => {
                    btn.disabled = false;
                    btn.textContent = "Import";
                });

        } catch (err) {
            alert("Failed to parse JSON file for verification.\nCheck file format.");
        }
    };
    reader.readAsText(file);
}

function validateUserInput(fc, cn, name, flag, currentIndex = -1) {
    if (!/^\d{1,9}$/.test(fc)) {
        alert("Error: Facility Code must be a number (1-9 digits).");
        return false;
    }
    if (!/^\d{1,9}$/.test(cn)) {
        alert("Error: Card Number must be a number (1-9 digits).");
        return false;
    }
    if (name.length > 12) {
        alert("Error: Name cannot exceed 12 characters.");
        return false;
    }
    if (flag.length > 21) {
        alert("Error: Flag cannot exceed 21 characters.");
        return false;
    }

    const fcVal = parseInt(fc, 10);
    const cnVal = parseInt(cn, 10);
    const isDuplicate = usersCache.some((user, idx) => {
        if (idx === currentIndex) return false;
        return user.facilityCode === fcVal && user.cardNumber === cnVal;
    });

    if (isDuplicate) {
        alert("Error: A user with this Facility Code and Card Number already exists.");
        return false;
    }

    return true;
}

function copyToClipboard(text) {
    const tempInput = document.createElement('input');
    tempInput.style.position = 'absolute';
    tempInput.style.left = '-9999px';
    tempInput.value = text;
    document.body.appendChild(tempInput);
    tempInput.select();
    document.execCommand('copy');
    document.body.removeChild(tempInput);
}

function rebootDevice() {
    if (confirm("Are you sure you want to reboot the device?")) {
        const btn = document.getElementById('navBtnReboot');
        if (btn) btn.classList.add('active');
        fetch('/rebootDevice', { method: 'POST' })
            .then(() => {
                alert("Device is rebooting. Please reconnect in ~10 seconds.");
                setTimeout(() => window.location.reload(), 5000);
            })
            .catch(err => {
                alert("Error sending reboot command.");
                if (btn) btn.classList.remove('active');
            });
    }
}

function checkChanges() {
    const warningEl = document.getElementById('rebootWarning');
    const pwdHintEl = document.getElementById('pwdHint');

    const currentPwd = document.getElementById('ap_pwd').value;
    const currentSsid = document.getElementById('ap_ssid').value;
    const currentHidden = document.getElementById('ssid_hidden').checked;

    const displayEl = document.getElementById('activeDisplayType');
    const flipEl = document.getElementById('flipOled');

    const currentDisplay = displayEl ? parseInt(displayEl.value, 10) : originalDisplayType;
    const currentFlip = flipEl ? flipEl.checked : originalFlipOled;

    if (currentPwd.length > 0 && currentPwd.length < 8) {
        pwdHintEl.classList.remove('hidden');
    } else {
        pwdHintEl.classList.add('hidden');
    }

    const wifiChanged = (currentPwd !== originalPwd) || (currentSsid !== originalSsid) || (currentHidden !== originalHidden);
    const displayChanged = (currentDisplay !== originalDisplayType) || (currentFlip !== originalFlipOled);

    if (wifiChanged || displayChanged) {
        warningEl.textContent = "Important settings have been changed! Device will reboot after saving.";
        warningEl.classList.remove('hidden');
        warningEl.style.display = '';
    } else {
        warningEl.classList.add('hidden');
        warningEl.style.display = '';
    }
}

function editUser(index) {
    const user = usersCache[index];
    if (!user) return;
    document.getElementById('newFacilityCode').value = user.facilityCode;
    document.getElementById('newCardNumber').value = user.cardNumber;
    document.getElementById('newName').value = user.name;
    document.getElementById('newFlag').value = user.flag || '';
    const saveBtn = document.getElementById('saveUserButton');
    const cancelBtn = document.getElementById('cancelEditButton');
    const clearBtn = document.getElementById('clearUserFieldsButton');
    if (saveBtn) {
        saveBtn.textContent = 'Update';
        saveBtn.onclick = function () { saveEditedUser(index); };
    }
    if (cancelBtn) cancelBtn.style.display = 'inline-block';
    if (clearBtn) clearBtn.style.display = 'none';
    checkUserInputsDirty();
}

function saveEditedUser(index) {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;
    const flag = document.getElementById('newFlag').value;

    if (!validateUserInput(facilityCode, cardNumber, name, flag, index)) return;

    fetch(`/updateUser?index=${index}&facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${encodeURIComponent(name)}&flag=${encodeURIComponent(flag)}&t=${Date.now()}`)
        .then(response => {
            if (response.ok) {
                updateUserTable().then(() => {
                    updateScanLog();
                });
                alert('User updated successfully');
                const saveBtn = document.getElementById('saveUserButton');
                const cancelBtn = document.getElementById('cancelEditButton');
                const clearBtn = document.getElementById('clearUserFieldsButton');
                if (saveBtn) { saveBtn.textContent = 'Save'; saveBtn.onclick = addUser; }
                if (cancelBtn) cancelBtn.style.display = 'none';
                if (clearBtn) clearBtn.style.display = 'inline-block';
            } else {
                alert('Failed to update user');
            }
        })
        .catch(error => console.error('Error updating card:', error));
}

function cancelEdit() {
    const saveBtn = document.getElementById('saveUserButton');
    const cancelBtn = document.getElementById('cancelEditButton');
    const clearBtn = document.getElementById('clearUserFieldsButton');
    if (saveBtn) { saveBtn.textContent = 'Save'; saveBtn.onclick = addUser; }
    if (cancelBtn) cancelBtn.style.display = 'none';
    if (clearBtn) clearBtn.style.display = 'inline-block';
    document.getElementById('newFacilityCode').value = '';
    document.getElementById('newCardNumber').value = '';
    document.getElementById('newName').value = '';
    document.getElementById('newFlag').value = '';
    checkUserInputsDirty();
}

function clearUserFields() {
    const fcInput = document.getElementById('newFacilityCode');
    const cnInput = document.getElementById('newCardNumber');
    const nameInput = document.getElementById('newName');
    const flagInput = document.getElementById('newFlag');
    if (fcInput) fcInput.value = '';
    if (cnInput) cnInput.value = '';
    if (nameInput) nameInput.value = '';
    if (flagInput) flagInput.value = '';
    checkUserInputsDirty();
}

function togglePasswordVisibility() {
    const pwdInput = document.getElementById('ap_pwd');
    const btn = document.getElementById('togglePwdBtn');

    if (pwdInput.type === 'password') {
        pwdInput.type = 'text';
        btn.innerHTML = `
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
                <circle cx="12" cy="12" r="3"></circle>
            </svg>`;
    } else {
        pwdInput.type = 'password';
        btn.innerHTML = `
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M1 1l22 22"></path>
                <path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"></path>
            </svg>`;
    }
}

document.getElementById('ap_ssid').addEventListener('input', () => { checkDirty(); checkChanges(); });
document.getElementById('ap_pwd').addEventListener('input', () => { checkDirty(); checkChanges(); });
document.getElementById('ssid_hidden').addEventListener('change', () => { checkDirty(); checkChanges(); });
document.getElementById('activeDisplayType').addEventListener('change', () => { checkDirty(); checkChanges(); });
document.getElementById('flipOled').addEventListener('change', () => { checkDirty(); checkChanges(); });

document.getElementById('modeSelect') && document.getElementById('modeSelect').addEventListener('change', checkDirty);
document.getElementById('timeoutSelect').addEventListener('change', checkDirty);
document.getElementById('ledValid').addEventListener('change', checkDirty);
document.getElementById('customMessage').addEventListener('input', checkDirty);
document.getElementById('enable_tamper_detect').addEventListener('change', checkDirty);
document.getElementById('disable_encoder').addEventListener('change', checkDirty);

setInterval(updateScanLog, 5000);
setInterval(fetchSettings, 5000);

window.onload = function () {
    if (!screenInterval) screenInterval = setInterval(updateScreen, 150);
    fetchSettings();
    
    // Load wiegand formats
    fetch('/wiegand_formats.json')
        .then(r => r.json())
        .then(data => {
            if (data && data.wiegandFormats) {
                wiegandFormats = data.wiegandFormats;
            }
        })
        .catch(err => console.error('Error loading wiegand formats:', err))
        .finally(() => {
            updateUserTable().finally(() => {
                updateScanLog();
            });
        });
};