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

function updateTable() {
    fetch('/getCards')
        .then(response => response.json())
        .then(data => {
            tableBody.innerHTML = '';
            cardData = [];
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

function updateUserTable() {
    fetch('/getUsers')
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

            // Add input row at the bottom of the table
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
    fetch('/getCards')
        .then(response => response.json())
        .then(data => {
            lastReadCardsTableBody.innerHTML = '';
            // show newest card first: reverse makes newest index zero
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
                // status cell
                cellStatus.innerHTML = card.status;

                // If status is RawRead or Unauthorized, show FC/CN/HEX/PAD in details
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

            // Add empty rows if there are less than 10 entries
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

    // validation check
    if (!validateUserInput(facilityCode, cardNumber, name, flag)) return;

    fetch(`/addUser?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${encodeURIComponent(name)}&flag=${encodeURIComponent(flag)}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                // Optional: Clear inputs on success
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

// When switching to the CTF Mode tab, refresh settings to get latest mode
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
    
    // 1 represents the LCD, hide flip
    // 2 and 3 are OLEDs, show flip
    if (displayType === "1") {
        container.style.display = 'none';
    } else {
        container.style.display = 'inline-block';
    }
}

function checkChanges() {
    const warningEl = document.getElementById('rebootWarning');
    const pwdHintEl = document.getElementById('pwdHint'); 
    
    // 1. Get current values for REBOOT triggers
    const currentPwd = document.getElementById('ap_pwd').value;
    const currentSsid = document.getElementById('ap_ssid').value;
    const currentHidden = document.getElementById('ssid_hidden').checked;
    
    // Safe checks for elements that might not exist yet
    const displayEl = document.getElementById('activeDisplayType');
    const flipEl = document.getElementById('flipOled');
    
    // Parse integers for comparison
    const currentDisplay = displayEl ? parseInt(displayEl.value, 10) : originalDisplayType;
    const currentFlip = flipEl ? flipEl.checked : originalFlipOled;

    // 2. Password Length Hint Logic
    if (currentPwd.length > 0 && currentPwd.length < 8) {
        pwdHintEl.classList.remove('hidden');
    } else {
        pwdHintEl.classList.add('hidden');
    }

    // 3. Change Detection (Strictly for Reboot items)
    const wifiChanged = (currentPwd !== originalPwd) || (currentSsid !== originalSsid) || (currentHidden !== originalHidden);
    const displayChanged = (currentDisplay !== originalDisplayType) || (currentFlip !== originalFlipOled);
    
    // 4. Update Warning Text
    if (wifiChanged || displayChanged) {
        warningEl.textContent = "Important settings have been changed! Device will reboot after saving.";
        warningEl.classList.remove('hidden');
        warningEl.style.display = ''; 
    } else {
        // If only non-reboot items (Mode, LED, etc) changed, hide the warning
        warningEl.classList.add('hidden');
        warningEl.style.display = 'none';
    }
}


function updateCTFIndicator(settings) {
    const mode = (settings.device_mode || settings.mode || '').toString().toLowerCase();
    // Require strict 'ctf' mode value from backend/frontend
    const isCTFOn = (mode === 'ctf');
    const statusEl = document.getElementById('ctfStatus');
    const hintEl = document.getElementById('ctfHint');
    if (statusEl) {
        statusEl.textContent = isCTFOn ? 'ON' : 'OFF';
        // toggle classes for background highlight
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
            // alarm state
            statusEl.textContent = 'TAMPERING DETECTED!';
            statusEl.classList.add('status-yellow');
        } else {
            // safe state
            statusEl.textContent = 'ENABLED';
            statusEl.classList.add('status-green');
        }
    }
}

function saveSettings() {
    const pwdInput = document.getElementById('ap_pwd').value;
    const ssidInput = document.getElementById('ap_ssid').value;
    const hiddenInput = document.getElementById('ssid_hidden').checked;

    // validations
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
    
    // detect changes
    const pwdChanged = (pwdInput !== originalPwd);
    const ssidChanged = (ssidInput !== originalSsid);
    const hiddenChanged = (hiddenInput !== originalHidden);
    const wifiChanged = (pwdChanged || ssidChanged || hiddenChanged);

    const currentDisplay = parseInt(document.getElementById('activeDisplayType').value, 10);
    const currentFlip = document.getElementById('flipOled').checked;
    
    const displayChanged = (currentDisplay !== originalDisplayType) || (currentFlip !== originalFlipOled);
    const rebootRequired = wifiChanged || displayChanged;

    // confirmation
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

    // gather settings
    const mode = document.getElementById('modeSelect').value.toString().toLowerCase();
    const timeout = document.getElementById('timeoutSelect').value;
    const customMessage = document.getElementById('customMessage').value;
    const ledValid = document.getElementById('ledValid').value;
    const activeDisplayType = document.getElementById('activeDisplayType').value;
    const enableTamperDetect = document.getElementById('enable_tamper_detect').checked;

    // build payload
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
        should_reboot: rebootRequired
    };

    // send
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
    fetch('/getSettings')
        .then(response => response.json())
        .then(data => {
            // 1. ALWAYS update the status badges (CTF / Tamper)
            // We want these live even while you are editing settings
            updateCTFIndicator(data);
            updateTamperIndicator(data);

            // 2. CONDITIONALLY update the form inputs
            const settingsTab = document.getElementById('settings');
            
            // Update the form ONLY if:
            // A. The settings tab is HIDDEN (you aren't looking at it), OR
            // B. Explicitly forced (e.g., right after hitting Save)
            if (settingsTab.classList.contains('hidden') || forceUpdateUI === true) {
                updateSettingsUI(data);
            }
        })
        .catch(error => console.error('Error fetching settings:', error));
}

window.onload = fetchSettings;
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
    // Check if the user wants to proceed
    if (confirm(`Download the current users.json configuration with ${currentUserCount} users?`)) {
        // If yes, trigger the download
        window.location.href = '/downloadUsers';
    }
}

function confirmSampleDownload() {
    // The \n creates a new line in the popup box
    const message = "Download sample file?\n\nMake sure to remove .sample extension and before re-uploading!";
    
    if (confirm(message)) {
        window.location.href = '/downloadSample';
    }
}


function importData() {
    const dataString = importExportArea.value;
    try {
        const data = JSON.parse(dataString);
        if (Array.isArray(data)) {
            data.forEach(card => {
                const facilityCode = card.facilityCode;
                const cardNumber = card.cardNumber;
                const name = card.name;
                const flag = card.flag || '';

                fetch(`/addUser?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${name}&flag=${encodeURIComponent(flag)}`)
                    .then(response => {
                        if (!response.ok) {
                            throw new Error('Failed to add card');
                        }
                    })
                    .catch(error => console.error('Error adding card:', error));
            });
            updateUserTable();
            alert('Data imported successfully');
        } else {
            alert('Invalid data format');
        }
    } catch (error) {
        alert('Invalid JSON format');
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

    // 1. Read the file locally to get the count
    reader.onload = function(e) {
        try {
            const json = JSON.parse(e.target.result);
            
            // Safety check: ensure 'users' array exists
            let count = 0;
            if (json.users && Array.isArray(json.users)) {
                count = json.users.length;
            } else {
                alert("Invalid file format: 'users' array is missing.");
                return;
            }

            // 2. ASK FOR CONFIRMATION
            const message = `Import ${count} users?\n\nWARNING: This will overwrite all current users!`;
            if (!confirm(message)) {
                // User clicked Cancel
                return; 
            }

            // 3. PROCEED WITH UPLOAD (User said Yes)
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
                    statusDiv.style.color = "#236b2b"; // Green
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
                statusDiv.style.color = "#8b1f1f"; // Red
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

    // Trigger the read
    reader.readAsText(file);
}

function validateUserInput(fc, cn, name, flag) {
    // FC: Number, 1-12 digits
    if (!/^\d{1,12}$/.test(fc)) {
        alert("Error: Facility Code must be a number (1-12 digits).");
        return false;
    }
    // CN: Number, 1-12 digits
    if (!/^\d{1,12}$/.test(cn)) {
        alert("Error: Card Number must be a number (1-12 digits).");
        return false;
    }
    // Name: Max 20 chars
    if (name.length > 20) {
        alert("Error: Name cannot exceed 20 characters.");
        return false;
    }
    // Flag: Max 20 chars
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
            // Optional: Reload page to force a reconnect attempt (will fail until device is back)
            setTimeout(() => window.location.reload(), 5000);
        })
        .catch(err => alert("Error sending reboot command."));
    }
}

setInterval(updateTable, 5000);
setInterval(updateLastReadCardsTable, 5000);
setInterval(fetchSettings, 5000);

updateTable();
updateUserTable();
updateLastReadCardsTable();

function checkChanges() {
    const warningEl = document.getElementById('rebootWarning');
    const pwdHintEl = document.getElementById('pwdHint'); 
    
    // 1. Get current values
    const currentPwd = document.getElementById('ap_pwd').value;
    const currentSsid = document.getElementById('ap_ssid').value;
    const currentHidden = document.getElementById('ssid_hidden').checked;
    
    // Safe checks for elements that might not exist yet if partial load
    const displayEl = document.getElementById('activeDisplayType');
    const flipEl = document.getElementById('flipOled');
    
    // Parse integers for comparison
    const currentDisplay = displayEl ? parseInt(displayEl.value, 10) : originalDisplayType;
    const currentFlip = flipEl ? flipEl.checked : originalFlipOled; // Fixed variable name

    // 2. Password Length Hint Logic
    if (currentPwd.length > 0 && currentPwd.length < 8) {
        pwdHintEl.classList.remove('hidden');
    } else {
        pwdHintEl.classList.add('hidden');
    }

    // 3. Change Detection
    const wifiChanged = (currentPwd !== originalPwd) || (currentSsid !== originalSsid) || (currentHidden !== originalHidden);
    const displayChanged = (currentDisplay !== originalDisplayType) || (currentFlip !== originalFlipOled);
    
    // 4. Update Warning Text
    if (wifiChanged || displayChanged) {
        // --- TEXT UPDATE HERE ---
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
                // reset buttons
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
    // clear inputs
    document.getElementById('newFacilityCode').value = '';
    document.getElementById('newCardNumber').value = '';
    document.getElementById('newName').value = '';
    document.getElementById('newFlag').value = '';
}

function togglePasswordVisibility() {
    const pwdInput = document.getElementById('ap_pwd');
    const btn = document.getElementById('togglePwdBtn');
    
    if (pwdInput.type === 'password') {
        // show password (open eye)
        pwdInput.type = 'text';
        btn.innerHTML = `
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
                <circle cx="12" cy="12" r="3"></circle>
            </svg>`;
    } else {
        // hidden (crossed eye)
        pwdInput.type = 'password';
        btn.innerHTML = `
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M1 1l22 22"></path>
                <path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"></path>
            </svg>`;
    }
}