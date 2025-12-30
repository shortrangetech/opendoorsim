let cardData = [];
const tableBody = document.getElementById('cardTable').getElementsByTagName('tbody')[0];
const userTableBody = document.getElementById('userTable').getElementsByTagName('tbody')[0];
const lastReadCardsTableBody = document.getElementById('lastReadCardsTable').getElementsByTagName('tbody')[0];
const importExportArea = document.getElementById('importExportArea');

let originalSsid = "";
let originalHidden = false;
let originalPwd = "";

let originalDisplayType = 1;
let originalFlipOled = false;

let currentUserCount = 0;
let sortNewestFirst = true;

// FIX 1: Track if the user is currently editing the form
let unsavedChanges = false;

// NEW globals for dirty checking
let originalMode = "raw";
let originalTimeout = 5000;
let originalCustomMessage = "";
let originalLedValid = 1;
let originalTamper = false;
let originalDisableEncoder = false;

// --- script.js ---

function checkDirty() {
    // 1. Gather Current Values
    const currSsid = document.getElementById('ap_ssid').value;
    const currPwd = document.getElementById('ap_pwd').value;
    const currHidden = document.getElementById('ssid_hidden').checked;
    
    const currMode = document.getElementById('modeSelect').value;
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
    
    if (currMode !== originalMode) isDirty = true;
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

function updateTable() {
    // FIX 2: Add timestamp to prevent caching
    fetch('/getCards?t=' + Date.now())
        .then(response => response.json())
        .then(data => {
            tableBody.innerHTML = '';
            cardData = [];

            if (sortNewestFirst) {
                data.reverse();
            }

            data.forEach((card, index) => {
                cardData.push(card);
                let row = tableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellBitLength = row.insertCell(1);
                let cellFacilityCode = row.insertCell(2);
                let cellCardNumber = row.insertCell(3);
                let cellRawData = row.insertCell(4);
                let cellHexData = row.insertCell(5);
                let cellPadCount = row.insertCell(6);
                cellIndex.innerHTML = index + 1;
                
                cellBitLength.innerHTML = card.bitCount;
                cellFacilityCode.innerHTML = card.facilityCode;
                cellCardNumber.innerHTML = card.cardNumber;
                cellRawData.innerHTML = `<a href="#" onclick="copyToClipboard('${card.rawCardData}')">${card.rawCardData}</a>`;
                cellHexData.innerHTML = card.hexData ? `<a href="#" onclick="copyToClipboard('${card.hexData}')">${card.hexData}</a>` : 'N/A';
                cellPadCount.innerHTML = card.padCount === 0 ? 'None' : card.padCount;
            });
        })
        .catch(error => console.error('Error fetching card data:', error));
}

function setSort(order) {
    sortNewestFirst = (order === 'new');
    const badgeNew = document.getElementById('badgeSortNew');
    const badgeOld = document.getElementById('badgeSortOld');

    if (sortNewestFirst) {
        badgeNew.className = "badge badge-purple badge-clickable";
        badgeOld.className = "badge badge-gray badge-clickable";
    } else {
        badgeNew.className = "badge badge-gray badge-clickable";
        badgeOld.className = "badge badge-purple badge-clickable";
    }
    updateTable();
}

function updateUserTable() {
    fetch('/getUsers?t=' + Date.now())
        .then(response => response.json())
        .then(data => {
            currentUserCount = data.length;
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

            cellFacilityCode.innerHTML = '<input type="number" id="newFacilityCode">';
            cellCardNumber.innerHTML = '<input type="number" id="newCardNumber">';
            cellName.innerHTML = '<input type="text" id="newName">';
            cellFlag.innerHTML = '<input type="text" id="newFlag">';
            cellAction.innerHTML = '<button id="saveUserButton" onclick="addUser()">Save</button> <button id="cancelEditButton" style="display:none;" onclick="cancelEdit()">Cancel</button>';
        })
        .catch(error => console.error('Error fetching user data:', error));
}

function updateLastReadCardsTable() {
    fetch('/getCards?t=' + Date.now())
        .then(response => response.json())
        .then(data => {
            lastReadCardsTableBody.innerHTML = '';
            const last10Cards = data.slice(-10).reverse();
            last10Cards.forEach((card, index) => {
                let row = lastReadCardsTableBody.insertRow();
                if (card.status === "Authorized") {
                    row.classList.add("authorized");
                } else if (card.status === "Unauthorized") {
                    row.classList.add("unauthorized");
                }
                let cellIndex = row.insertCell(0);
                let cellStatus = row.insertCell(1);
                let cellDetails = row.insertCell(2);

                cellIndex.innerHTML = index + 1;
                cellStatus.innerHTML = card.status;

                if (card.status === 'RawRead' || card.status === 'Unauthorized') {
                    const fc = (card.facilityCode !== undefined) ? card.facilityCode : '';
                    const cn = (card.cardNumber !== undefined) ? card.cardNumber : '';
                    const hex = card.hexData || 'N/A';
                    const pad = (card.padCount === 0) ? 'None' : card.padCount;
                    cellDetails.innerHTML = `FC: ${fc} &nbsp; CN: ${cn} &nbsp; HEX: ${hex} &nbsp; PAD: ${pad}`;
                } else {
                    cellDetails.innerHTML = card.details || '';
                }
            });

            for (let i = last10Cards.length; i < 10; i++) {
                let row = lastReadCardsTableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellStatus = row.insertCell(1);
                let cellDetails = row.insertCell(2);
                cellIndex.innerHTML = i + 1;
                cellStatus.innerHTML = "";
                cellDetails.innerHTML = "";
            }
        })
        .catch(error => console.error('Error fetching last read card data:', error));
}

function addUser() {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;
    const flag = document.getElementById('newFlag').value;

    if (!validateUserInput(facilityCode, cardNumber, name, flag)) return;

    fetch(`/addUser?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${encodeURIComponent(name)}&flag=${encodeURIComponent(flag)}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
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
    fetch(`/deleteUser?index=${index}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('User deleted successfully');
            } else {
                alert('Failed to delete user');
            }
        })
        .catch(error => console.error('Error deleting user:', error));
}

function showSection(section) {
    document.getElementById('log').classList.add('hidden');
    document.getElementById('monitor').classList.add('hidden');
    document.getElementById('settings').classList.add('hidden');
    document.getElementById(section).classList.remove('hidden');
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

function updateCTFIndicator(settings) {
    const mode = (settings.device_mode || settings.mode || '').toString().toLowerCase();
    const isCTFOn = (mode === 'ctf');
    const statusEl = document.getElementById('ctfStatus');
    const hintEl = document.getElementById('ctfHint');
    if (statusEl) {
        statusEl.textContent = isCTFOn ? 'ON' : 'OFF';
        statusEl.classList.toggle('ctf-on', isCTFOn);
        statusEl.classList.toggle('ctf-off', !isCTFOn);
    }
    if (hintEl) hintEl.style.display = isCTFOn ? 'none' : 'block';
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

    const mode = document.getElementById('modeSelect').value.toString().toLowerCase();
    const timeout = document.getElementById('timeoutSelect').value;
    const customMessage = document.getElementById('customMessage').value;
    const ledValid = document.getElementById('ledValid').value;
    const activeDisplayType = document.getElementById('activeDisplayType').value;
    const enableTamperDetect = document.getElementById('enable_tamper_detect').checked;

    let settings = {
        device_mode: mode,
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
            updateCTFIndicator(data);
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

    // 1. Populate UI
    if (document.getElementById('modeSelect')) document.getElementById('modeSelect').value = (mode || '').toString().toLowerCase();
    
    const modeValueEl = document.getElementById('modeValue');
    if (modeValueEl) modeValueEl.textContent = (mode || '').toString().toUpperCase();

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

    // 2. STORE ORIGINAL VALUES
    originalSsid = apSsid;
    originalPwd = apPass;
    originalHidden = (ssidHidden == 1 || ssidHidden === true);
    originalDisplayType = activeDisplayType;
    originalFlipOled = settings.flip_oled_display;
    
    
    // New globals
    originalMode = (mode || '').toString().toLowerCase();
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
    
    // Run the check immediately to reset the button state
    checkDirty(); 
}

function exportData() {
    fetch('/getUsers')
        .then(response => response.json())
        .then(data => {
            const dataString = JSON.stringify(data);
            importExportArea.value = dataString;
            importExportArea.select();
            document.execCommand('copy');
            alert('Data exported to clipboard');
        })
        .catch(error => console.error('Error exporting data:', error));
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

    reader.onload = function(e) {
        try {
            const json = JSON.parse(e.target.result);
            let count = 0;
            if (json.users && Array.isArray(json.users)) {
                count = json.users.length;
            } else {
                alert("Invalid file format: 'users' array is missing.");
                return;
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

function validateUserInput(fc, cn, name, flag) {
    if (!/^\d{1,12}$/.test(fc)) {
        alert("Error: Facility Code must be a number (1-12 digits).");
        return false;
    }
    if (!/^\d{1,12}$/.test(cn)) {
        alert("Error: Card Number must be a number (1-12 digits).");
        return false;
    }
    if (name.length > 20) {
        alert("Error: Name cannot exceed 20 characters.");
        return false;
    }
    if (flag.length > 20) {
        alert("Error: Flag cannot exceed 20 characters.");
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
        fetch('/rebootDevice', { method: 'POST' })
        .then(() => {
            alert("Device is rebooting. Please reconnect in ~10 seconds.");
            setTimeout(() => window.location.reload(), 5000);
        })
        .catch(err => alert("Error sending reboot command."));
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
    fetch('/getUsers')
        .then(res => res.json())
        .then(data => {
            const user = data[index];
            if (!user) return;
            document.getElementById('newFacilityCode').value = user.facilityCode;
            document.getElementById('newCardNumber').value = user.cardNumber;
            document.getElementById('newName').value = user.name;
            document.getElementById('newFlag').value = user.flag || '';
            const saveBtn = document.getElementById('saveUserButton');
            const cancelBtn = document.getElementById('cancelEditButton');
            if (saveBtn) {
                saveBtn.textContent = 'Update';
                saveBtn.onclick = function() { saveEditedUser(index); };
            }
            if (cancelBtn) cancelBtn.style.display = 'inline-block';
        })
        .catch(err => console.error('Error fetching users for edit:', err));
}

function saveEditedUser(index) {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;
    const flag = document.getElementById('newFlag').value;

    if (!validateUserInput(facilityCode, cardNumber, name, flag)) return;

    fetch(`/updateUser?index=${index}&facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${encodeURIComponent(name)}&flag=${encodeURIComponent(flag)}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('User updated successfully');
                const saveBtn = document.getElementById('saveUserButton');
                const cancelBtn = document.getElementById('cancelEditButton');
                if (saveBtn) { saveBtn.textContent = 'Save'; saveBtn.onclick = addUser; }
                if (cancelBtn) cancelBtn.style.display = 'none';
            } else {
                alert('Failed to update user');
            }
        })
        .catch(error => console.error('Error updating card:', error));
}

function cancelEdit() {
    const saveBtn = document.getElementById('saveUserButton');
    const cancelBtn = document.getElementById('cancelEditButton');
    if (saveBtn) { saveBtn.textContent = 'Save'; saveBtn.onclick = addUser; }
    if (cancelBtn) cancelBtn.style.display = 'none';
    document.getElementById('newFacilityCode').value = '';
    document.getElementById('newCardNumber').value = '';
    document.getElementById('newName').value = '';
    document.getElementById('newFlag').value = '';
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

document.getElementById('modeSelect').addEventListener('change', checkDirty);
document.getElementById('timeoutSelect').addEventListener('change', checkDirty);
document.getElementById('ledValid').addEventListener('change', checkDirty);
document.getElementById('customMessage').addEventListener('input', checkDirty);
document.getElementById('enable_tamper_detect').addEventListener('change', checkDirty);
document.getElementById('disable_encoder').addEventListener('change', checkDirty);

setInterval(updateTable, 5000);
setInterval(updateLastReadCardsTable, 5000);
setInterval(fetchSettings, 5000);

window.onload = function() {
    fetchSettings();
    updateTable();
    updateUserTable();
    updateLastReadCardsTable();
};